#include "layout_check.h"
#include "measured_layout.h"

#include <engine/client.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/entity.h>
#include <game/collision.h>
#include <game/gamecore.h>

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

AF_EXPOSE(GameClientClient, CGameClient, m_pClient, IClient *);
AF_EXPOSE(GameClientConfig, CGameClient, m_pConfig, CConfig *);
AF_EXPOSE(GameClientCollision, CGameClient, m_Collision, CCollision);
AF_EXPOSE(GameClientControls, CGameClient, m_Controls, CControls);
AF_EXPOSE(GameClientSnap, CGameClient, m_Snap, CGameClient::CSnapState);
AF_EXPOSE(GameClientLocalIds, CGameClient, m_aLocalIds, int[NUM_DUMMIES]);
AF_EXPOSE(GameClientPredictedWorld, CGameClient, m_PredictedWorld, CGameWorld);
using ClientDataArray = CGameClient::CClientData[MAX_CLIENTS];
using TuningArray = CTuningParams[TuneZone::NUM];
AF_EXPOSE(GameClientClientData, CGameClient, m_aClients, ClientDataArray);
AF_EXPOSE(GameClientTuningList, CGameClient, m_aTuningList, TuningArray);
AF_EXPOSE(ClientDataPredictedCore, CGameClient::CClientData, m_Predicted, CCharacterCore);

AF_EXPOSE(GameWorldTick, CGameWorld, m_GameTick, int);
AF_EXPOSE(GameWorldFlags, CGameWorld, m_WorldConfig, decltype(CGameWorld::m_WorldConfig));
AF_EXPOSE(GameWorldLocalId, CGameWorld, m_LocalClientId, int);
AF_EXPOSE(GameWorldEgoTiles, CGameWorld, m_pEgoTilesPrediction, CEgoTilesPrediction *);
AF_EXPOSE(GameWorldEntityLists, CGameWorld, m_apFirstEntityTypes, CEntity *[CGameWorld::NUM_ENTTYPES]);
AF_EXPOSE(GameWorldCharacters, CGameWorld, m_apCharacters, CCharacter *[MAX_CLIENTS]);
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

AF_EXPOSE(CorePosition, CCharacterCore, m_Pos, vec2);
AF_EXPOSE(CoreVelocity, CCharacterCore, m_Vel, vec2);
AF_EXPOSE(CoreHookPosition, CCharacterCore, m_HookPos, vec2);
AF_EXPOSE(CoreHookDirection, CCharacterCore, m_HookDir, vec2);
AF_EXPOSE(CoreHookTeleBase, CCharacterCore, m_HookTeleBase, vec2);
AF_EXPOSE(CoreHookTick, CCharacterCore, m_HookTick, int);
AF_EXPOSE(CoreHookState, CCharacterCore, m_HookState, int);
AF_EXPOSE(CoreInput, CCharacterCore, m_Input, CNetObj_PlayerInput);
AF_EXPOSE(CoreTuning, CCharacterCore, m_Tuning, CTuningParams);

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

void ValidateMeasuredLayout()
{
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

    CheckOffset(NativeMemberOffset(Access(CorePosition{})), measured::CORE_POSITION, "CCharacterCore::m_Pos");
    CheckOffset(NativeMemberOffset(Access(CoreVelocity{})), measured::CORE_VELOCITY, "CCharacterCore::m_Vel");
    CheckOffset(NativeMemberOffset(Access(CoreHookPosition{})), measured::CORE_HOOK_POSITION, "CCharacterCore::m_HookPos");
    CheckOffset(NativeMemberOffset(Access(CoreHookDirection{})), measured::CORE_HOOK_DIRECTION, "CCharacterCore::m_HookDir");
    CheckOffset(NativeMemberOffset(Access(CoreHookTeleBase{})), measured::CORE_HOOK_TELE_BASE, "CCharacterCore::m_HookTeleBase");
    CheckOffset(NativeMemberOffset(Access(CoreHookTick{})), measured::CORE_HOOK_TICK, "CCharacterCore::m_HookTick");
    CheckOffset(NativeMemberOffset(Access(CoreHookState{})), measured::CORE_HOOK_STATE, "CCharacterCore::m_HookState");
    CheckOffset(NativeMemberOffset(Access(CoreInput{})), measured::CORE_INPUT, "CCharacterCore::m_Input");
    CheckOffset(NativeMemberOffset(Access(CoreTuning{})), measured::CORE_TUNING, "CCharacterCore::m_Tuning");
}
} // namespace avoidfreeze::win32
