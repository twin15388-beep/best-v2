#include "avoid_policy.h"

#include <game/teamscore.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace avoidfreeze
{
namespace
{
struct Risk
{
    int first_tick{};
};

void Require(bool condition, const char *message)
{
    if(!condition)
        throw std::runtime_error(message);
}

const char *MissingCaptureCheck(const CaptureChecks &checks)
{
    if(!checks.exact_public_image) return "exact audited public executable profile is required";
    if(!checks.native_layout_matches) return "measured native layout checks did not pass";
    if(!checks.unique_root) return "exactly one validated client root is required";
    if(!checks.online) return "client is not in the supported online state";
    if(!checks.local_ids_match) return "local player IDs or prediction ticks disagree";
    if(!checks.one_active_player) return "only one active local player is supported";
    if(!checks.no_other_prediction_entities) return "other predicted characters/entities are unsupported";
    if(!checks.no_unsupported_collision_layers) return "dynamic collision layers are unsupported";
    if(!checks.no_ego_tile_prediction) return "ego-tile prediction is unsupported";
    if(!checks.supported_prediction_mode) return "client prediction mode is unsupported";
    if(!checks.stable_double_read) return "snapshot changed during bounded double-read";
    return nullptr;
}

bool IsHazardTile(int tile) noexcept
{
    return tile == TILE_DEATH || tile == TILE_FREEZE || tile == TILE_DFREEZE || tile == TILE_LFREEZE;
}

bool InMap(const TileMapSnapshot &map, vec2 position) noexcept
{
    if(!std::isfinite(position.x) || !std::isfinite(position.y))
        return false;
    const float max_x = static_cast<float>(map.width) * 32.0f;
    const float max_y = static_cast<float>(map.height) * 32.0f;
    return position.x >= 0.0f && position.y >= 0.0f && position.x < max_x && position.y < max_y;
}

bool HazardAt(CCollision &collision, vec2 position)
{
    const int index = collision.GetPureMapIndex(position);
    const auto cell_count = static_cast<std::int64_t>(collision.GetWidth()) * collision.GetHeight();
    Require(index >= 0 && static_cast<std::int64_t>(index) < cell_count,
            "predicted position left the supported map bounds");
    return IsHazardTile(collision.GetTileIndex(index)) || IsHazardTile(collision.GetFrontTileIndex(index));
}

Risk Simulate(const CharacterState &player, CCollision &collision, int direction)
{
    Require(direction >= -1 && direction <= 1, "candidate direction is outside -1..1");

    CWorldCore world{};
    CTeamsCore teams;
    teams.Reset();
    CCharacterCore core{};
    core.Init(&world, &collision, &teams);
    core.Reset();

    core.m_Id = player.client_id;
    core.m_Pos = player.position;
    core.m_Vel = player.velocity;
    core.m_HookPos = player.hook_position;
    core.m_HookDir = player.hook_direction;
    core.m_HookTeleBase = player.hook_tele_base;
    core.m_HookTick = player.hook_tick;
    core.m_HookState = player.hook_state;
    core.m_ActiveWeapon = player.active_weapon;
    core.m_NewHook = player.new_hook;
    core.m_Jumped = player.jumped;
    core.m_JumpedTotal = player.jumped_total;
    core.m_Jumps = player.jumps;
    core.m_Direction = player.direction;
    core.m_Angle = player.angle;
    core.m_Input = player.input;
    core.m_Input.m_Direction = direction;
    core.m_TriggeredEvents = player.triggered_events;
    core.m_Reset = player.reset;
    core.m_Colliding = player.colliding;
    core.m_LeftWall = player.left_wall;
    core.m_Solo = player.solo;
    core.m_Jetpack = player.jetpack;
    core.m_CollisionDisabled = player.collision_disabled;
    core.m_EndlessHook = player.endless_hook;
    core.m_EndlessJump = player.endless_jump;
    core.m_HammerHitDisabled = player.hammer_hit_disabled;
    core.m_GrenadeHitDisabled = player.grenade_hit_disabled;
    core.m_LaserHitDisabled = player.laser_hit_disabled;
    core.m_ShotgunHitDisabled = player.shotgun_hit_disabled;
    core.m_HookHitDisabled = player.hook_hit_disabled;
    core.m_Super = player.super;
    core.m_Invincible = player.invincible;
    core.m_HasTelegunGun = player.has_telegun;
    core.m_HasTelegunGrenade = player.has_telegrenade;
    core.m_HasTelegunLaser = player.has_telelaser;
    core.m_FreezeStart = player.freeze_start;
    core.m_FreezeEnd = player.freeze_end;
    core.m_IsInFreeze = player.in_freeze;
    core.m_DeepFrozen = player.deep_frozen;
    core.m_LiveFrozen = player.live_frozen;
    core.m_Tuning = player.tuning;
    teams.SetSolo(player.client_id, player.solo);
    world.m_apCharacters[player.client_id] = &core;

    if(HazardAt(collision, core.m_Pos))
        return {1};

    for(int tick = 1; tick <= PREDICTION_HORIZON_TICKS; ++tick)
    {
        const vec2 before = core.m_Pos;
        core.m_Input.m_Direction = direction;
        core.Tick(true);
        core.Move();

        Require(std::isfinite(core.m_Pos.x) && std::isfinite(core.m_Pos.y) &&
                    std::isfinite(core.m_Vel.x) && std::isfinite(core.m_Vel.y),
                "pinned physics produced a non-finite candidate state");
        const float map_width = static_cast<float>(collision.GetWidth()) * 32.0f;
        const float map_height = static_cast<float>(collision.GetHeight()) * 32.0f;
        if(!(core.m_Pos.x >= 0.0f && core.m_Pos.y >= 0.0f &&
               core.m_Pos.x < map_width && core.m_Pos.y < map_height))
            throw std::runtime_error("predicted position left the map: x=" + std::to_string(core.m_Pos.x) +
                                     " y=" + std::to_string(core.m_Pos.y));

        const auto traversed = collision.GetMapIndices(before, core.m_Pos, 256);
        Require(traversed.size() < 256, "predicted swept path exceeds the tile-query budget");
        const auto cell_count = static_cast<std::int64_t>(collision.GetWidth()) * collision.GetHeight();
        for(const int tile : traversed)
        {
            Require(tile >= 0 && static_cast<std::int64_t>(tile) < cell_count,
                    "predicted swept path left the map bounds");
            if(IsHazardTile(collision.GetTileIndex(tile)) || IsHazardTile(collision.GetFrontTileIndex(tile)))
                return {tick};
        }
        if(HazardAt(collision, core.m_Pos) || core.m_IsInFreeze || core.m_DeepFrozen || core.m_LiveFrozen)
            return {tick};
    }
    return {};
}

void ValidatePlayer(const CharacterState &player, const TileMapSnapshot &map)
{
    Require(player.client_id >= 0 && player.client_id < MAX_CLIENTS, "local client ID is outside the pinned protocol range");
    Require(player.tick >= 0 && player.tick < std::numeric_limits<int>::max() - PREDICTION_HORIZON_TICKS - 1,
            "prediction tick is outside the bounded simulation range");
    Require(player.freeze_time >= 0, "freeze timer is negative");
    Require(InMap(map, player.position), "current character position is outside the measured map");
    Require(std::isfinite(player.velocity.x) && std::isfinite(player.velocity.y) &&
                std::abs(player.velocity.x) <= 1024.0f && std::abs(player.velocity.y) <= 1024.0f,
            "current velocity is outside the local computation guard");
    Require(player.direction >= -1 && player.direction <= 1, "character direction is outside -1..1");
    Require(player.input.m_Direction >= -1 && player.input.m_Direction <= 1, "predicted input direction is outside -1..1");
    Require(player.input.m_Jump == 0 && player.input.m_Hook == 0 && player.input.m_Fire == 0 &&
                player.input.m_NextWeapon == 0 && player.input.m_PrevWeapon == 0 && player.input.m_WantedWeapon == 0,
            "jump, hook, fire, or weapon input is not modeled in this preview");
    Require((player.input.m_PlayerFlags & (PLAYERFLAG_IN_MENU | PLAYERFLAG_CHATTING)) == 0,
            "menu/chat input state is not supported");
    Require(player.hook_state == HOOK_IDLE && !player.new_hook,
            "active/retracting/teleported hook state is not supported");
    Require(player.active_weapon >= 0 && player.active_weapon < NUM_WEAPONS && player.active_weapon != WEAPON_NINJA,
            "ninja or invalid weapon state is not supported");
    Require(player.jumped >= 0 && player.jumped <= 3 && player.jumped_total >= 0 && player.jumped_total <= 100000 &&
                player.jumps >= -1 && player.jumps <= 100000,
            "jump state is outside the pinned physics import bounds");
    Require(!player.solo && !player.jetpack && !player.collision_disabled && !player.endless_hook &&
                !player.endless_jump && !player.hammer_hit_disabled && !player.grenade_hit_disabled &&
                !player.laser_hit_disabled && !player.shotgun_hit_disabled && !player.hook_hit_disabled &&
                !player.has_telegun && !player.has_telegrenade && !player.has_telelaser,
            "special character mechanics are not supported by this static-world policy");

    const CTuningParams &tuning = player.tuning;
    for(int index = 0; index < CTuningParams::Num(); ++index)
        Require(tuning.NetworkArray()[index] >= -1000000 && tuning.NetworkArray()[index] <= 1000000,
                "tuning value exceeds the local arithmetic guard");
    Require(tuning.m_VelrampRange > 0.0f && tuning.m_VelrampCurvature > 0.0f &&
                tuning.m_GroundControlSpeed >= 0.0f && tuning.m_GroundControlAccel >= 0.0f &&
                tuning.m_AirControlSpeed >= 0.0f && tuning.m_AirControlAccel >= 0.0f,
            "tuning values are outside the supported arithmetic domain");
}
} // namespace

Decision Analyze(const SnapshotFrame &frame)
{
    Decision decision;
    decision.current_direction = frame.player.input.m_Direction;
    decision.suggested_direction = decision.current_direction;

    try
    {
        if(const char *missing = MissingCaptureCheck(frame.checks))
            throw std::runtime_error(missing);
        ValidateStaticMap(frame.map);
        ValidatePlayer(frame.player, frame.map);

        if(frame.player.freeze_time > 0 || frame.player.in_freeze || frame.player.deep_frozen || frame.player.live_frozen)
        {
            decision.kind = DecisionKind::already_frozen;
            decision.reason = "player is already frozen; this advisory policy will not suggest a recovery action";
            return decision;
        }
        if(frame.player.super || frame.player.invincible)
        {
            decision.kind = DecisionKind::immune;
            decision.reason = "pinned character state reports freeze immunity; no correction is suggested";
            return decision;
        }

        PhysicsMap map(frame.map);
        const Risk baseline = Simulate(frame.player, map.Collision(), frame.player.input.m_Direction);
        decision.predicted_hazard_tick = baseline.first_tick;
        if(baseline.first_tick == 0)
        {
            decision.kind = DecisionKind::no_predicted_hazard;
            decision.reason = "no supported freeze/death tile is predicted within the bounded horizon";
            return decision;
        }

        for(const int candidate : {0, -1, 1})
        {
            if(candidate == frame.player.input.m_Direction)
                continue;
            const Risk risk = Simulate(frame.player, map.Collision(), candidate);
            if(risk.first_tick == 0)
            {
                decision.kind = DecisionKind::suggested_direction;
                decision.suggested_direction = candidate;
                decision.reason = "pinned single-character physics found a no-hazard direction in the bounded horizon";
                return decision;
            }
        }

        decision.kind = DecisionKind::no_safe_direction;
        decision.reason = "all modeled directions reach a freeze/death tile within the horizon";
    }
    catch(const std::exception &error)
    {
        decision.kind = DecisionKind::unavailable;
        decision.reason = error.what();
    }
    return decision;
}
} // namespace avoidfreeze
