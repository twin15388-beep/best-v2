// Exact Best public v3.1.1/cf192e3; SHA256 check is in ../core.h.
// All following values are OBJECT-relative byte offsets, NOT module RVAs.
// MSVC19.44/x64 compiler: runtime acquisition43017692183/job130512762553;
// config/nested ABI acquisition43019848266/job130520014168 (layout step passed;
// later isolated build setup failed; do not claim that entire run passed).
// Ego empty-vector ABI: run43020413963/layout step passed; later link failed.
// Live no-input checks: local/core/tick/collision/tuning/freeze on owned map.
// Neither remote authorization nor arbitrary dynamic-world parity is implied.
#pragma once
#include <cstddef>
#include <optional>
namespace RemoteLayout {
namespace CCharacter {
inline constexpr std::size_t m_IsLocal = 96;
inline constexpr std::size_t m_NinjaJetpack = 97;
inline constexpr std::size_t m_FreezeTime = 100;
inline constexpr std::size_t m_FrozenLastTick = 104;
inline constexpr std::size_t m_PrevPos = 108;
inline constexpr std::size_t m_PrevPrevPos = 116;
inline constexpr std::size_t m_TeleCheckpoint = 124;
inline constexpr std::size_t m_TileIndex = 128;
inline constexpr std::size_t m_TileFIndex = 132;
inline constexpr std::size_t m_LastRefillJumps = 136;
inline constexpr std::size_t m_LastSnapWeapon = 140;
inline constexpr std::size_t m_KeepHooked = 144;
inline constexpr std::size_t m_GameTeam = 148;
inline constexpr std::size_t m_CanMoveInFreeze = 152;
inline constexpr std::size_t m_FreezeAccumulation = 156;
inline constexpr std::size_t m_AliveAccumulation = 160;
inline constexpr std::size_t m_aHitObjects = 164;
inline constexpr std::size_t m_NumObjectsHit = 676;
inline constexpr std::size_t m_LastWeapon = 680;
inline constexpr std::size_t m_QueuedWeapon = 684;
inline constexpr std::size_t m_ReloadTimer = 688;
inline constexpr std::size_t m_AttackTick = 692;
inline constexpr std::size_t m_MoveRestrictions = 696;
inline constexpr std::size_t m_PainSoundTimer = 700;
inline constexpr std::size_t m_LatestPrevInput = 704;
inline constexpr std::size_t m_LatestInput = 744;
inline constexpr std::size_t m_PrevInput = 784;
inline constexpr std::size_t m_Input = 824;
inline constexpr std::size_t m_SavedInput = 864;
inline constexpr std::size_t m_NumInputs = 904;
inline constexpr std::size_t m_TuneZone = 908;
inline constexpr std::size_t m_TuneZoneOverride = 912;
inline constexpr std::size_t m_Core = 920;
inline constexpr std::size_t m_StrongWeakId = 1496;
inline constexpr std::size_t m_LastWeaponSwitchTick = 1500;
inline constexpr std::size_t m_LastTuneZoneTick = 1504;
inline constexpr std::size_t m_Interfering = 1508;
}
namespace CCharacterCore {
inline constexpr std::size_t m_pWorld = 0;
inline constexpr std::size_t m_pCollision = 8;
inline constexpr std::size_t m_Pos = 16;
inline constexpr std::size_t m_Vel = 24;
inline constexpr std::size_t m_HookPos = 32;
inline constexpr std::size_t m_HookDir = 40;
inline constexpr std::size_t m_HookTeleBase = 48;
inline constexpr std::size_t m_HookTick = 56;
inline constexpr std::size_t m_HookState = 60;
inline constexpr std::size_t m_AttachedPlayers = 64;
inline constexpr std::size_t m_ActiveWeapon = 80;
inline constexpr std::size_t m_aWeapons = 84;
inline constexpr std::size_t m_Ninja = 180;
inline constexpr std::size_t m_NewHook = 200;
inline constexpr std::size_t m_Jumped = 204;
inline constexpr std::size_t m_JumpedTotal = 208;
inline constexpr std::size_t m_Jumps = 212;
inline constexpr std::size_t m_Direction = 216;
inline constexpr std::size_t m_Angle = 220;
inline constexpr std::size_t m_Input = 224;
inline constexpr std::size_t m_TriggeredEvents = 264;
inline constexpr std::size_t m_Id = 268;
inline constexpr std::size_t m_Reset = 272;
inline constexpr std::size_t m_Colliding = 276;
inline constexpr std::size_t m_LeftWall = 280;
inline constexpr std::size_t m_Solo = 281;
inline constexpr std::size_t m_Jetpack = 282;
inline constexpr std::size_t m_CollisionDisabled = 283;
inline constexpr std::size_t m_EndlessHook = 284;
inline constexpr std::size_t m_EndlessJump = 285;
inline constexpr std::size_t m_HammerHitDisabled = 286;
inline constexpr std::size_t m_GrenadeHitDisabled = 287;
inline constexpr std::size_t m_LaserHitDisabled = 288;
inline constexpr std::size_t m_ShotgunHitDisabled = 289;
inline constexpr std::size_t m_HookHitDisabled = 290;
inline constexpr std::size_t m_Super = 291;
inline constexpr std::size_t m_Invincible = 292;
inline constexpr std::size_t m_HasTelegunGun = 293;
inline constexpr std::size_t m_HasTelegunGrenade = 294;
inline constexpr std::size_t m_HasTelegunLaser = 295;
inline constexpr std::size_t m_FreezeStart = 296;
inline constexpr std::size_t m_FreezeEnd = 300;
inline constexpr std::size_t m_IsInFreeze = 304;
inline constexpr std::size_t m_DeepFrozen = 305;
inline constexpr std::size_t m_LiveFrozen = 306;
inline constexpr std::size_t m_Tuning = 308;
inline constexpr std::size_t m_pTeams = 496;
inline constexpr std::size_t m_MoveRestrictions = 504;
inline constexpr std::size_t m_HookedPlayer = 508;
inline constexpr std::size_t m_AntiPingInterfereCallback = 512;
inline constexpr std::size_t m_Active = 0;
inline constexpr std::size_t m_Prev = 4;
inline constexpr std::size_t m_Cur = 92;
inline constexpr std::size_t m_ExtendedData = 180;
inline constexpr std::size_t m_pPrevExtendedData = 224;
inline constexpr std::size_t m_HasExtendedData = 232;
inline constexpr std::size_t m_HasExtendedDisplayInfo = 233;
}
namespace CEntity {
inline constexpr std::size_t m_pPrevTypeEntity = 8;
inline constexpr std::size_t m_pNextTypeEntity = 16;
inline constexpr std::size_t m_pGameWorld = 24;
inline constexpr std::size_t m_MarkedForDestroy = 32;
inline constexpr std::size_t m_Id = 36;
inline constexpr std::size_t m_ObjType = 40;
inline constexpr std::size_t m_ProximityRadius = 44;
inline constexpr std::size_t m_Pos = 48;
inline constexpr std::size_t m_Number = 56;
inline constexpr std::size_t m_Layer = 60;
inline constexpr std::size_t m_SnapTicks = 64;
inline constexpr std::size_t m_DestroyTick = 68;
inline constexpr std::size_t m_LastRenderTick = 72;
inline constexpr std::size_t m_pParent = 80;
inline constexpr std::size_t m_pChild = 88;
}
namespace CGameClient {
inline constexpr std::size_t m_Binds = 4485816;
inline constexpr std::size_t m_EgoTilesPrediction = 5447272;
inline constexpr std::size_t m_Controls = 8525880;
inline constexpr std::size_t m_pClient = 9026824;
inline constexpr std::size_t m_pConfig = 9026848;
inline constexpr std::size_t m_Collision = 9027024;
inline constexpr std::size_t m_Snap = 9030736;
inline constexpr std::size_t m_aClients = 9084832;
inline constexpr std::size_t m_aLocalIds = 10988508;
inline constexpr std::size_t m_PredictedWorld = 10992424;
inline constexpr std::size_t m_aTuningList = 11013512;
}
namespace CGameWorld {
inline constexpr std::size_t m_GameTick = 1700;
inline constexpr std::size_t m_WorldConfig = 1704;
inline constexpr std::size_t m_LocalClientId = 1752;
inline constexpr std::size_t m_pEgoTilesPrediction = 1760;
inline constexpr std::size_t m_apFirstEntityTypes = 1800;
inline constexpr std::size_t m_apCharacters = 2120;
inline constexpr std::size_t m_pCollision = 3144;
inline constexpr std::size_t m_pTuningList = 3152;
}
namespace IClient {
inline constexpr std::size_t m_State = 7784;
inline constexpr std::size_t m_aCurGameTick = 7808;
inline constexpr std::size_t m_aPredTick = 7840;
}
namespace CCollision {
inline constexpr std::size_t m_Width = 16;
inline constexpr std::size_t m_Height = 20;
inline constexpr std::size_t m_pTiles = 24;
inline constexpr std::size_t m_pTele = 32;
inline constexpr std::size_t m_pSpeedup = 40;
inline constexpr std::size_t m_pFront = 48;
inline constexpr std::size_t m_pSwitch = 56;
inline constexpr std::size_t m_pTune = 64;
inline constexpr std::size_t m_pDoor = 72;
inline constexpr std::size_t m_pExtraDoorTiles = 80;
}
namespace CBinds {
inline constexpr std::size_t m_aapKeyBindings = 48;
}
namespace CEgoTilesPrediction { inline constexpr std::optional<std::size_t> m_vQuads{16}; }
namespace CConfig {
inline constexpr std::size_t m_ClDummy = 3692;
inline constexpr std::size_t m_ClAntiPingPlayers = 24;
inline constexpr std::size_t m_ClPredictFreeze = 15016;
inline constexpr std::size_t m_SvOldTeleportHook = 4728;
inline constexpr std::size_t m_SvOldTeleportWeapons = 4724;
inline constexpr std::size_t m_SvTeam = 12680;
inline constexpr std::size_t m_ClPredictEvents = 8;
inline constexpr std::size_t m_SndEnable = 6604;
inline constexpr std::size_t m_SvDeepfly = 4740;
inline constexpr std::size_t m_SvDraggerRange = 14160;
inline constexpr std::size_t m_SvFreezeDelay = 10396;
inline constexpr std::size_t m_SvHit = 10384;
inline constexpr std::size_t m_SvTeleportHoldHook = 4732;
inline constexpr std::size_t m_TcUnfreezeLagDelayTicks = 16196;
}
namespace Snap {
inline constexpr std::size_t m_LocalClientId = 22736;
inline constexpr std::size_t m_NumPlayers = 22740;
inline constexpr std::size_t m_aCharacters = 22792;
}
namespace SnapCharacter {
inline constexpr std::size_t m_Active = 0;
}
namespace ClientData {
inline constexpr std::size_t m_Predicted = 120;
}
namespace Controls {
inline constexpr std::size_t m_aInputData = 104;
}
namespace WorldConfig {
inline constexpr std::size_t m_IsDDRace = 1704;
inline constexpr std::size_t m_IsVanilla = 1705;
inline constexpr std::size_t m_IsFNG = 1706;
inline constexpr std::size_t m_InfiniteAmmo = 1707;
inline constexpr std::size_t m_PredictTiles = 1708;
inline constexpr std::size_t m_PredictFreeze = 1712;
inline constexpr std::size_t m_PredictWeapons = 1716;
inline constexpr std::size_t m_PredictDDRace = 1717;
inline constexpr std::size_t m_IsSolo = 1718;
inline constexpr std::size_t m_UseTuneZones = 1719;
inline constexpr std::size_t m_BugDDRaceInput = 1720;
inline constexpr std::size_t m_NoWeakHookAndBounce = 1721;
inline constexpr std::size_t m_PredictEvents = 1722;
inline constexpr std::size_t m_OldLaser = 1723;
inline constexpr std::size_t m_PredictTeleports = 1724;
}
inline constexpr std::size_t sizeof_CCharacter = 1512;
inline constexpr std::size_t sizeof_CCharacterCore = 576;
inline constexpr std::size_t sizeof_CTuningParams = 188;
inline constexpr std::size_t sizeof_CNetObj_PlayerInput = 40;
inline constexpr std::size_t sizeof_CGameClient_CClientData = 14784;
inline constexpr std::size_t sizeof_CGameClient_CSnapState_CCharacterInfo = 240;
inline constexpr std::size_t sizeof_CConfig = 26492;
}
