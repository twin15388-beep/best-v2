#pragma once

#include "static_map.h"

#include <game/gamecore.h>

#include <string>

namespace avoidfreeze
{
struct CharacterState
{
    int client_id{-1};
    int tick{-1};
    int freeze_time{};

    vec2 position{};
    vec2 velocity{};
    vec2 hook_position{};
    vec2 hook_direction{};
    vec2 hook_tele_base{};
    int hook_tick{};
    int hook_state{};
    int active_weapon{};
    bool new_hook{};
    int jumped{};
    int jumped_total{};
    int jumps{};
    int direction{};
    int angle{};
    CNetObj_PlayerInput input{};
    int triggered_events{};
    bool reset{};
    int colliding{};
    bool left_wall{};
    bool solo{};
    bool jetpack{};
    bool collision_disabled{};
    bool endless_hook{};
    bool endless_jump{};
    bool hammer_hit_disabled{};
    bool grenade_hit_disabled{};
    bool laser_hit_disabled{};
    bool shotgun_hit_disabled{};
    bool hook_hit_disabled{};
    bool super{};
    bool invincible{};
    bool has_telegun{};
    bool has_telegrenade{};
    bool has_telelaser{};
    int freeze_start{};
    int freeze_end{};
    bool in_freeze{};
    bool deep_frozen{};
    bool live_frozen{};
    CTuningParams tuning{};
};

struct CaptureChecks
{
    bool exact_public_image{};
    bool native_layout_matches{};
    bool unique_root{};
    bool online{};
    bool local_ids_match{};
    bool one_active_player{};
    bool no_other_prediction_entities{};
    bool no_unsupported_collision_layers{};
    bool no_ego_tile_prediction{};
    bool supported_prediction_mode{};
    bool stable_double_read{};
};

struct SnapshotFrame
{
    TileMapSnapshot map;
    CharacterState player;
    CaptureChecks checks;
    std::string capture_note;
};
} // namespace avoidfreeze
