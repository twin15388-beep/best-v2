#include "layout_check.h"
#include "measured_layout.h"

#include <engine/client.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/entity.h>
#include <game/collision.h>
#include <game/gamecore.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace avoidfreeze::win32
{
namespace
{
template<class Tag, typename Tag::Type Member>
struct PrivateMember
{
    friend constexpr typename Tag::Type Access(Tag) { return Member; }
};

#define AF_EXPOSE(TagName, Owner, Member, ValueType)                  \
    struct TagName                                                    \
    {                                                                 \
        using Type = ValueType Owner::*;                              \
        friend constexpr Type Access(TagName);                        \
    };                                                                \
    template struct PrivateMember<TagName, &Owner::Member>

using LocalIdsArray = int[NUM_DUMMIES];
using PredictedTicksArray = int[NUM_DUMMIES];
using WorldConfig = decltype(CGameWorld::m_WorldConfig);
AF_EXPOSE(ClientState, IClient, m_State, IClient::EClientState);
AF_EXPOSE(ClientPredictedTicks, IClient, m_aPredTick, PredictedTicksArray);
AF_EXPOSE(GameClientClient, CGameClient, m_pClient, IClient *);
AF_EXPOSE(GameClientConfig, CGameClient, m_pConfig, CConfig *);
AF_EXPOSE(GameClientCollision, CGameClient, m_Collision, CCollision);
AF_EXPOSE(GameClientControls, CGameClient, m_Controls, CControls);
AF_EXPOSE(GameClientSnap, CGameClient, m_Snap, CGameClient::CSnapState);
AF_EXPOSE(GameClientLocalIds, CGameClient, m_aLocalIds, LocalIdsArray);
AF_EXPOSE(GameClientPredictedWorld, CGameClient, m_PredictedWorld, CGameWorld);
using ClientDataArray = CGameClient::CClientData[MAX_CLIENTS];
using TuningArray = CTuningParams[TuneZone::NUM];
using EntityListArray = CEntity *[CGameWorld::NUM_ENTTYPES];
using CharacterArray = CCharacter *[MAX_CLIENTS];
AF_EXPOSE(GameClientClientData, CGameClient, m_aClients, ClientDataArray);
AF_EXPOSE(GameClientTuningList, CGameClient, m_aTuningList, TuningArray);
AF_EXPOSE(ClientDataPredictedCore, CGameClient::CClientData, m_Predicted, CCharacterCore);

AF_EXPOSE(GameWorldTick, CGameWorld, m_GameTick, int);
AF_EXPOSE(GameWorldFlags, CGameWorld, m_WorldConfig, WorldConfig);
AF_EXPOSE(GameWorldLocalId, CGameWorld, m_LocalClientId, int);
AF_EXPOSE(GameWorldEgoTiles, CGameWorld, m_pEgoTilesPrediction, CEgoTilesPrediction *);
AF_EXPOSE(GameWorldEntityLists, CGameWorld, m_apFirstEntityTypes, EntityListArray);
AF_EXPOSE(GameWorldCharacters, CGameWorld, m_apCharacters, CharacterArray);
AF_EXPOSE(GameWorldCollision, CGameWorld, m_pCollision, CCollision *);
AF_EXPOSE(GameWorldTuning, CGameWorld, m_pTuningList, CTuningParams *);

AF_EXPOSE(EntityWorld, CEntity, m_pGameWorld, CGameWorld *);
AF_EXPOSE(EntityId, CEntity, m_Id, int);
AF_EXPOSE(EntityPosition, CEntity, m_Pos, vec2);
AF_EXPOSE(CharacterFreezeTime, CCharacter, m_FreezeTime, int);
AF_EXPOSE(CharacterLocal, CCharacter, m_IsLocal, bool);
AF_EXPOSE(CharacterCore, CCharacter, m_Core, CCharacterCore);

AF_EXPOSE(CollisionWidth, CCollision, m_Width, int);
AF_EXPOSE(CollisionHeight, CCollision, m_Height, int);
AF_EXPOSE(CollisionGameTiles, CCollision, m_pTiles, CTile *);
AF_EXPOSE(CollisionTele, CCollision, m_pTele, CTeleTile *);
AF_EXPOSE(CollisionSpeedup, CCollision, m_pSpeedup, CSpeedupTile *);
AF_EXPOSE(CollisionFront, CCollision, m_pFront, CTile *);
AF_EXPOSE(CollisionSwitch, CCollision, m_pSwitch, CSwitchTile *);
AF_EXPOSE(CollisionTune, CCollision, m_pTune, CTuneTile *);
AF_EXPOSE(CollisionDoor, CCollision, m_pDoor, CDoorTile *);
AF_EXPOSE(CollisionExtraDoor, CCollision, m_pExtraDoorTiles, const CDoorTile *);

AF_EXPOSE(CoreWorld, CCharacterCore, m_pWorld, CWorldCore *);
AF_EXPOSE(CoreCollision, CCharacterCore, m_pCollision, CCollision *);
AF_EXPOSE(CorePosition, CCharacterCore, m_Pos, vec2);
AF_EXPOSE(CoreVelocity, CCharacterCore, m_Vel, vec2);
AF_EXPOSE(CoreHookPosition, CCharacterCore, m_HookPos, vec2);
AF_EXPOSE(CoreHookDirection, CCharacterCore, m_HookDir, vec2);
AF_EXPOSE(CoreHookTeleBase, CCharacterCore, m_HookTeleBase, vec2);
AF_EXPOSE(CoreHookTick, CCharacterCore, m_HookTick, int);
AF_EXPOSE(CoreHookState, CCharacterCore, m_HookState, int);
AF_EXPOSE(CoreActiveWeapon, CCharacterCore, m_ActiveWeapon, int);
AF_EXPOSE(CoreNewHook, CCharacterCore, m_NewHook, bool);
AF_EXPOSE(CoreJumped, CCharacterCore, m_Jumped, int);
AF_EXPOSE(CoreJumpedTotal, CCharacterCore, m_JumpedTotal, int);
AF_EXPOSE(CoreJumps, CCharacterCore, m_Jumps, int);
AF_EXPOSE(CoreDirection, CCharacterCore, m_Direction, int);
AF_EXPOSE(CoreAngle, CCharacterCore, m_Angle, int);
AF_EXPOSE(CoreInput, CCharacterCore, m_Input, CNetObj_PlayerInput);
AF_EXPOSE(CoreEvents, CCharacterCore, m_TriggeredEvents, int);
AF_EXPOSE(CoreId, CCharacterCore, m_Id, int);
AF_EXPOSE(CoreReset, CCharacterCore, m_Reset, bool);
AF_EXPOSE(CoreColliding, CCharacterCore, m_Colliding, int);
AF_EXPOSE(CoreLeftWall, CCharacterCore, m_LeftWall, bool);
AF_EXPOSE(CoreSolo, CCharacterCore, m_Solo, bool);
AF_EXPOSE(CoreJetpack, CCharacterCore, m_Jetpack, bool);
AF_EXPOSE(CoreCollisionDisabled, CCharacterCore, m_CollisionDisabled, bool);
AF_EXPOSE(CoreEndlessHook, CCharacterCore, m_EndlessHook, bool);
AF_EXPOSE(CoreEndlessJump, CCharacterCore, m_EndlessJump, bool);
AF_EXPOSE(CoreHammerDisabled, CCharacterCore, m_HammerHitDisabled, bool);
AF_EXPOSE(CoreGrenadeDisabled, CCharacterCore, m_GrenadeHitDisabled, bool);
AF_EXPOSE(CoreLaserDisabled, CCharacterCore, m_LaserHitDisabled, bool);
AF_EXPOSE(CoreShotgunDisabled, CCharacterCore, m_ShotgunHitDisabled, bool);
AF_EXPOSE(CoreHookDisabled, CCharacterCore, m_HookHitDisabled, bool);
AF_EXPOSE(CoreSuper, CCharacterCore, m_Super, bool);
AF_EXPOSE(CoreInvincible, CCharacterCore, m_Invincible, bool);
AF_EXPOSE(CoreTelegun, CCharacterCore, m_HasTelegunGun, bool);
AF_EXPOSE(CoreTelegrenade, CCharacterCore, m_HasTelegunGrenade, bool);
AF_EXPOSE(CoreTelelaser, CCharacterCore, m_HasTelegunLaser, bool);
AF_EXPOSE(CoreFreezeStart, CCharacterCore, m_FreezeStart, int);
AF_EXPOSE(CoreFreezeEnd, CCharacterCore, m_FreezeEnd, int);
AF_EXPOSE(CoreInFreeze, CCharacterCore, m_IsInFreeze, bool);
AF_EXPOSE(CoreDeepFrozen, CCharacterCore, m_DeepFrozen, bool);
AF_EXPOSE(CoreLiveFrozen, CCharacterCore, m_LiveFrozen, bool);
AF_EXPOSE(CoreTuning, CCharacterCore, m_Tuning, CTuningParams);
AF_EXPOSE(CoreHookedPlayer, CCharacterCore, m_HookedPlayer, int);

#undef AF_EXPOSE

template<typename MemberPointer>
std::size_t NativeMemberOffset(MemberPointer member)
{
    static_assert(sizeof(MemberPointer) == sizeof(std::int32_t),
                  "member pointer is not the measured MSVC single-inheritance x64 representation");
    std::int32_t offset{};
    std::memcpy(&offset, &member, sizeof(offset));
    if(offset < 0)
        throw std::runtime_error("native member offset is negative");
    return static_cast<std::size_t>(offset);
}

void CheckOffset(std::size_t actual, std::size_t expected, const char *field)
{
    if(actual != expected)
        throw std::runtime_error(std::string("measured MSVC layout mismatch: ") + field);
}
} // namespace

static_assert(sizeof(void *) == 8);
static_assert(sizeof(CGameClient) == measured::GAMECLIENT_SIZE);
static_assert(sizeof(CGameWorld) == measured::WORLD_SIZE);
static_assert(sizeof(CCharacter) == measured::CHARACTER_SIZE);
static_assert(sizeof(CCharacterCore) == measured::CORE_SIZE);
static_assert(sizeof(CCollision) == measured::COLLISION_SIZE);
static_assert(sizeof(CNetObj_PlayerInput) == measured::PLAYER_INPUT_SIZE);
static_assert(sizeof(CTuningParams) == measured::TUNING_SIZE);
static_assert(sizeof(CConfig) == measured::CONFIG_SIZE);
static_assert(sizeof(CGameClient::CClientData) == measured::CLIENT_DATA_SIZE);
static_assert(offsetof(CGameClient::CSnapState, m_LocalClientId) == measured::SNAP_LOCAL_CLIENT_ID);
static_assert(offsetof(CGameClient::CSnapState, m_NumPlayers) == measured::SNAP_PLAYER_COUNT);
static_assert(offsetof(CGameClient::CSnapState, m_aCharacters) == measured::SNAP_CHARACTERS);
static_assert(offsetof(CGameClient::CSnapState::CCharacterInfo, m_Active) == 0);
static_assert(sizeof(CGameClient::CSnapState::CCharacterInfo) == measured::SNAP_CHARACTER_SIZE);
static_assert(offsetof(CConfig, m_ClDummy) == measured::CONFIG_DUMMY);
static_assert(sizeof(WorldConfig) == 24);
static_assert(std::is_standard_layout_v<WorldConfig>);
static_assert(offsetof(WorldConfig, m_IsDDRace) == 0);
static_assert(offsetof(WorldConfig, m_IsVanilla) == 1);
static_assert(offsetof(WorldConfig, m_IsFNG) == 2);
static_assert(offsetof(WorldConfig, m_InfiniteAmmo) == 3);
static_assert(offsetof(WorldConfig, m_PredictTiles) == 4);
static_assert(offsetof(WorldConfig, m_PredictFreeze) == 8);
static_assert(offsetof(WorldConfig, m_PredictWeapons) == 12);
static_assert(offsetof(WorldConfig, m_PredictDDRace) == 13);
static_assert(offsetof(WorldConfig, m_IsSolo) == 14);
static_assert(offsetof(WorldConfig, m_UseTuneZones) == 15);
static_assert(offsetof(WorldConfig, m_BugDDRaceInput) == 16);
static_assert(offsetof(WorldConfig, m_NoWeakHookAndBounce) == 17);
static_assert(offsetof(WorldConfig, m_PredictEvents) == 18);
static_assert(offsetof(WorldConfig, m_OldLaser) == 19);
static_assert(offsetof(WorldConfig, m_PredictTeleports) == 20);
static_assert(std::is_standard_layout_v<CNetObj_PlayerInput>);
static_assert(offsetof(CNetObj_PlayerInput, m_Direction) == 0);
static_assert(offsetof(CNetObj_PlayerInput, m_TargetX) == 4);
static_assert(offsetof(CNetObj_PlayerInput, m_TargetY) == 8);
static_assert(offsetof(CNetObj_PlayerInput, m_Jump) == 12);
static_assert(offsetof(CNetObj_PlayerInput, m_Fire) == 16);
static_assert(offsetof(CNetObj_PlayerInput, m_Hook) == 20);
static_assert(offsetof(CNetObj_PlayerInput, m_PlayerFlags) == 24);
static_assert(offsetof(CNetObj_PlayerInput, m_WantedWeapon) == 28);
static_assert(offsetof(CNetObj_PlayerInput, m_NextWeapon) == 32);
static_assert(offsetof(CNetObj_PlayerInput, m_PrevWeapon) == 36);
static_assert(sizeof(CTuneParam) == sizeof(std::int32_t));

void ValidateMeasuredLayout()
{
    CheckOffset(NativeMemberOffset(Access(ClientState{})), measured::CLIENT_STATE, "IClient::m_State");
    CheckOffset(NativeMemberOffset(Access(ClientPredictedTicks{})), measured::CLIENT_PREDICTED_TICKS, "IClient::m_aPredTick");
    CheckOffset(NativeMemberOffset(Access(GameClientClient{})), measured::GAMECLIENT_CLIENT, "CGameClient::m_pClient");
    CheckOffset(NativeMemberOffset(Access(GameClientConfig{})), measured::GAMECLIENT_CONFIG, "CGameClient::m_pConfig");
    CheckOffset(NativeMemberOffset(Access(GameClientCollision{})), measured::GAMECLIENT_COLLISION, "CGameClient::m_Collision");
    CheckOffset(NativeMemberOffset(Access(GameClientControls{})), measured::GAMECLIENT_CONTROLS, "CGameClient::m_Controls");
    CheckOffset(NativeMemberOffset(Access(GameClientSnap{})), measured::GAMECLIENT_SNAP, "CGameClient::m_Snap");
    CheckOffset(NativeMemberOffset(Access(GameClientLocalIds{})), measured::GAMECLIENT_LOCAL_IDS, "CGameClient::m_aLocalIds");
    CheckOffset(NativeMemberOffset(Access(GameClientPredictedWorld{})), measured::GAMECLIENT_PREDICTED_WORLD, "CGameClient::m_PredictedWorld");
    CheckOffset(NativeMemberOffset(Access(GameClientClientData{})), measured::GAMECLIENT_CLIENT_DATA, "CGameClient::m_aClients");
    CheckOffset(NativeMemberOffset(Access(GameClientTuningList{})), measured::GAMECLIENT_TUNING_LIST, "CGameClient::m_aTuningList");
    CheckOffset(NativeMemberOffset(Access(ClientDataPredictedCore{})), measured::CLIENT_DATA_PREDICTED_CORE, "CClientData::m_Predicted");

    CheckOffset(NativeMemberOffset(Access(GameWorldTick{})), measured::WORLD_GAME_TICK, "CGameWorld::m_GameTick");
    CheckOffset(NativeMemberOffset(Access(GameWorldFlags{})), measured::WORLD_FLAGS, "CGameWorld::m_WorldConfig");
    CheckOffset(NativeMemberOffset(Access(GameWorldLocalId{})), measured::WORLD_LOCAL_CLIENT_ID, "CGameWorld::m_LocalClientId");
    CheckOffset(NativeMemberOffset(Access(GameWorldEgoTiles{})), measured::WORLD_EGO_TILES, "CGameWorld::m_pEgoTilesPrediction");
    CheckOffset(NativeMemberOffset(Access(GameWorldEntityLists{})), measured::WORLD_ENTITY_LISTS, "CGameWorld::m_apFirstEntityTypes");
    CheckOffset(NativeMemberOffset(Access(GameWorldCharacters{})), measured::WORLD_CHARACTERS, "CGameWorld::m_apCharacters");
    CheckOffset(NativeMemberOffset(Access(GameWorldCollision{})), measured::WORLD_COLLISION, "CGameWorld::m_pCollision");
    CheckOffset(NativeMemberOffset(Access(GameWorldTuning{})), measured::WORLD_TUNING, "CGameWorld::m_pTuningList");

    CheckOffset(NativeMemberOffset(Access(EntityWorld{})), measured::ENTITY_GAME_WORLD, "CEntity::m_pGameWorld");
    CheckOffset(NativeMemberOffset(Access(EntityId{})), measured::ENTITY_ID, "CEntity::m_Id");
    CheckOffset(NativeMemberOffset(Access(EntityPosition{})), measured::ENTITY_POSITION, "CEntity::m_Pos");
    CheckOffset(NativeMemberOffset(Access(CharacterLocal{})), measured::CHARACTER_LOCAL, "CCharacter::m_IsLocal");
    CheckOffset(NativeMemberOffset(Access(CharacterFreezeTime{})), measured::CHARACTER_FREEZE_TIME, "CCharacter::m_FreezeTime");
    CheckOffset(NativeMemberOffset(Access(CharacterCore{})), measured::CHARACTER_CORE, "CCharacter::m_Core");

    CheckOffset(NativeMemberOffset(Access(CollisionWidth{})), measured::COLLISION_WIDTH, "CCollision::m_Width");
    CheckOffset(NativeMemberOffset(Access(CollisionHeight{})), measured::COLLISION_HEIGHT, "CCollision::m_Height");
    CheckOffset(NativeMemberOffset(Access(CollisionGameTiles{})), measured::COLLISION_GAME_TILES, "CCollision::m_pTiles");
    CheckOffset(NativeMemberOffset(Access(CollisionTele{})), measured::COLLISION_TELE_LAYER, "CCollision::m_pTele");
    CheckOffset(NativeMemberOffset(Access(CollisionSpeedup{})), measured::COLLISION_SPEEDUP_LAYER, "CCollision::m_pSpeedup");
    CheckOffset(NativeMemberOffset(Access(CollisionFront{})), measured::COLLISION_FRONT_LAYER, "CCollision::m_pFront");
    CheckOffset(NativeMemberOffset(Access(CollisionSwitch{})), measured::COLLISION_SWITCH_LAYER, "CCollision::m_pSwitch");
    CheckOffset(NativeMemberOffset(Access(CollisionTune{})), measured::COLLISION_TUNE_LAYER, "CCollision::m_pTune");
    CheckOffset(NativeMemberOffset(Access(CollisionDoor{})), measured::COLLISION_DOOR_LAYER, "CCollision::m_pDoor");
    CheckOffset(NativeMemberOffset(Access(CollisionExtraDoor{})), measured::COLLISION_EXTRA_DOOR_LAYER, "CCollision::m_pExtraDoorTiles");

    CheckOffset(NativeMemberOffset(Access(CoreWorld{})), measured::CORE_WORLD, "CCharacterCore::m_pWorld");
    CheckOffset(NativeMemberOffset(Access(CoreCollision{})), measured::CORE_COLLISION, "CCharacterCore::m_pCollision");
    CheckOffset(NativeMemberOffset(Access(CorePosition{})), measured::CORE_POSITION, "CCharacterCore::m_Pos");
    CheckOffset(NativeMemberOffset(Access(CoreVelocity{})), measured::CORE_VELOCITY, "CCharacterCore::m_Vel");
    CheckOffset(NativeMemberOffset(Access(CoreHookPosition{})), measured::CORE_HOOK_POSITION, "CCharacterCore::m_HookPos");
    CheckOffset(NativeMemberOffset(Access(CoreHookDirection{})), measured::CORE_HOOK_DIRECTION, "CCharacterCore::m_HookDir");
    CheckOffset(NativeMemberOffset(Access(CoreHookTeleBase{})), measured::CORE_HOOK_TELE_BASE, "CCharacterCore::m_HookTeleBase");
    CheckOffset(NativeMemberOffset(Access(CoreHookTick{})), measured::CORE_HOOK_TICK, "CCharacterCore::m_HookTick");
    CheckOffset(NativeMemberOffset(Access(CoreHookState{})), measured::CORE_HOOK_STATE, "CCharacterCore::m_HookState");
    CheckOffset(NativeMemberOffset(Access(CoreActiveWeapon{})), measured::CORE_ACTIVE_WEAPON, "CCharacterCore::m_ActiveWeapon");
    CheckOffset(NativeMemberOffset(Access(CoreNewHook{})), measured::CORE_NEW_HOOK, "CCharacterCore::m_NewHook");
    CheckOffset(NativeMemberOffset(Access(CoreJumped{})), measured::CORE_JUMPED, "CCharacterCore::m_Jumped");
    CheckOffset(NativeMemberOffset(Access(CoreJumpedTotal{})), measured::CORE_JUMPED_TOTAL, "CCharacterCore::m_JumpedTotal");
    CheckOffset(NativeMemberOffset(Access(CoreJumps{})), measured::CORE_JUMPS, "CCharacterCore::m_Jumps");
    CheckOffset(NativeMemberOffset(Access(CoreDirection{})), measured::CORE_DIRECTION, "CCharacterCore::m_Direction");
    CheckOffset(NativeMemberOffset(Access(CoreAngle{})), measured::CORE_ANGLE, "CCharacterCore::m_Angle");
    CheckOffset(NativeMemberOffset(Access(CoreInput{})), measured::CORE_INPUT, "CCharacterCore::m_Input");
    CheckOffset(NativeMemberOffset(Access(CoreEvents{})), measured::CORE_EVENTS, "CCharacterCore::m_TriggeredEvents");
    CheckOffset(NativeMemberOffset(Access(CoreId{})), measured::CORE_ID, "CCharacterCore::m_Id");
    CheckOffset(NativeMemberOffset(Access(CoreReset{})), measured::CORE_RESET, "CCharacterCore::m_Reset");
    CheckOffset(NativeMemberOffset(Access(CoreColliding{})), measured::CORE_COLLIDING, "CCharacterCore::m_Colliding");
    CheckOffset(NativeMemberOffset(Access(CoreLeftWall{})), measured::CORE_LEFT_WALL, "CCharacterCore::m_LeftWall");
    CheckOffset(NativeMemberOffset(Access(CoreSolo{})), measured::CORE_SOLO, "CCharacterCore::m_Solo");
    CheckOffset(NativeMemberOffset(Access(CoreJetpack{})), measured::CORE_JETPACK, "CCharacterCore::m_Jetpack");
    CheckOffset(NativeMemberOffset(Access(CoreCollisionDisabled{})), measured::CORE_COLLISION_DISABLED, "CCharacterCore::m_CollisionDisabled");
    CheckOffset(NativeMemberOffset(Access(CoreEndlessHook{})), measured::CORE_ENDLESS_HOOK, "CCharacterCore::m_EndlessHook");
    CheckOffset(NativeMemberOffset(Access(CoreEndlessJump{})), measured::CORE_ENDLESS_JUMP, "CCharacterCore::m_EndlessJump");
    CheckOffset(NativeMemberOffset(Access(CoreHammerDisabled{})), measured::CORE_HAMMER_DISABLED, "CCharacterCore::m_HammerHitDisabled");
    CheckOffset(NativeMemberOffset(Access(CoreGrenadeDisabled{})), measured::CORE_GRENADE_DISABLED, "CCharacterCore::m_GrenadeHitDisabled");
    CheckOffset(NativeMemberOffset(Access(CoreLaserDisabled{})), measured::CORE_LASER_DISABLED, "CCharacterCore::m_LaserHitDisabled");
    CheckOffset(NativeMemberOffset(Access(CoreShotgunDisabled{})), measured::CORE_SHOTGUN_DISABLED, "CCharacterCore::m_ShotgunHitDisabled");
    CheckOffset(NativeMemberOffset(Access(CoreHookDisabled{})), measured::CORE_HOOK_DISABLED, "CCharacterCore::m_HookHitDisabled");
    CheckOffset(NativeMemberOffset(Access(CoreSuper{})), measured::CORE_SUPER, "CCharacterCore::m_Super");
    CheckOffset(NativeMemberOffset(Access(CoreInvincible{})), measured::CORE_INVINCIBLE, "CCharacterCore::m_Invincible");
    CheckOffset(NativeMemberOffset(Access(CoreTelegun{})), measured::CORE_TELEGUN, "CCharacterCore::m_HasTelegunGun");
    CheckOffset(NativeMemberOffset(Access(CoreTelegrenade{})), measured::CORE_TELEGRENADE, "CCharacterCore::m_HasTelegunGrenade");
    CheckOffset(NativeMemberOffset(Access(CoreTelelaser{})), measured::CORE_TELELASER, "CCharacterCore::m_HasTelegunLaser");
    CheckOffset(NativeMemberOffset(Access(CoreFreezeStart{})), measured::CORE_FREEZE_START, "CCharacterCore::m_FreezeStart");
    CheckOffset(NativeMemberOffset(Access(CoreFreezeEnd{})), measured::CORE_FREEZE_END, "CCharacterCore::m_FreezeEnd");
    CheckOffset(NativeMemberOffset(Access(CoreInFreeze{})), measured::CORE_IN_FREEZE, "CCharacterCore::m_IsInFreeze");
    CheckOffset(NativeMemberOffset(Access(CoreDeepFrozen{})), measured::CORE_DEEP_FROZEN, "CCharacterCore::m_DeepFrozen");
    CheckOffset(NativeMemberOffset(Access(CoreLiveFrozen{})), measured::CORE_LIVE_FROZEN, "CCharacterCore::m_LiveFrozen");
    CheckOffset(NativeMemberOffset(Access(CoreTuning{})), measured::CORE_TUNING, "CCharacterCore::m_Tuning");
    CheckOffset(NativeMemberOffset(Access(CoreHookedPlayer{})), measured::CORE_HOOKED_PLAYER, "CCharacterCore::m_HookedPlayer");
}
} // namespace avoidfreeze::win32
