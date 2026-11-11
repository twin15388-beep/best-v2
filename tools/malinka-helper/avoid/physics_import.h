#pragma once
#include "physics_remote_layout.h"
#include <game/client/prediction/entities/character.h>
#include <bit>
#include <span>
#include <cstdint>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Replica {
class Rejected : public std::runtime_error { public: using std::runtime_error::runtime_error; };
inline void Need(bool Test, const char *Why) { if(!Test) throw Rejected(Why); }
inline std::uint32_t U32(std::span<const std::uint8_t> Bytes, std::size_t At)
{
    Need(At <= Bytes.size() && Bytes.size()-At >= 4,"primitive field bound");
    return std::uint32_t(Bytes[At]) | std::uint32_t(Bytes[At+1])<<8 | std::uint32_t(Bytes[At+2])<<16 | std::uint32_t(Bytes[At+3])<<24;
}
inline std::int32_t Int(std::span<const std::uint8_t> Bytes, std::size_t At) { return std::bit_cast<std::int32_t>(U32(Bytes,At)); }
inline std::uint64_t U64(std::span<const std::uint8_t> Bytes, std::size_t At) { return U32(Bytes,At) | std::uint64_t(U32(Bytes,At+4))<<32; }
inline bool Bool(std::span<const std::uint8_t> Bytes, std::size_t At)
{
    Need(At < Bytes.size() && Bytes[At] <= 1,"noncanonical bool"); return Bytes[At]!=0;
}
inline float Float(std::span<const std::uint8_t> Bytes, std::size_t At)
{
    const float Result=std::bit_cast<float>(U32(Bytes,At));
    Need(std::isfinite(Result) && std::abs(Result)<1e9f,"nonfinite or excessive scalar");return Result;
}
inline vec2 Vector(std::span<const std::uint8_t> Bytes, std::size_t At) { return {Float(Bytes,At),Float(Bytes,At+4)}; }
inline CNetObj_PlayerInput Input(std::span<const std::uint8_t> Bytes, std::size_t At)
{
    CNetObj_PlayerInput Result{};
    Result.m_Direction=Int(Bytes,At);Result.m_TargetX=Int(Bytes,At+4);Result.m_TargetY=Int(Bytes,At+8);
    Result.m_Jump=Int(Bytes,At+12);Result.m_Fire=Int(Bytes,At+16);Result.m_Hook=Int(Bytes,At+20);
    Result.m_PlayerFlags=Int(Bytes,At+24);Result.m_WantedWeapon=Int(Bytes,At+28);Result.m_NextWeapon=Int(Bytes,At+32);Result.m_PrevWeapon=Int(Bytes,At+36);
    Need(Result.m_Direction>=-1 && Result.m_Direction<=1,"input direction");
    Need(Result.m_Jump>=0 && Result.m_Jump<=1 && Result.m_Hook>=0 && Result.m_Hook<=1,"input boolean range");
    Need(Result.m_TargetX>-1000000 && Result.m_TargetX<1000000 && Result.m_TargetY>-1000000 && Result.m_TargetY<1000000,"bounded aim");
    return Result;
}
// This never touches the target process. Explicit template instantiation has
// the standard access-check exception; no #define private or ODR mismatch.
template<class Tag, typename Tag::Type Member> struct PrivateField {
    friend constexpr typename Tag::Type Access(Tag) { return Member; }
};
#define REPLICA_ACCESS(Value, Owner, Member) \
struct Owner##Member { using FieldType=Value; using Type=FieldType Owner::*; friend constexpr Type Access(Owner##Member); }; \
template struct PrivateField<Owner##Member,&Owner::Member>;
REPLICA_ACCESS(int,CCharacter,m_NumObjectsHit)
REPLICA_ACCESS(int,CCharacter,m_LastWeapon)
REPLICA_ACCESS(int,CCharacter,m_QueuedWeapon)
REPLICA_ACCESS(int,CCharacter,m_ReloadTimer)
REPLICA_ACCESS(int,CCharacter,m_AttackTick)
REPLICA_ACCESS(int,CCharacter,m_MoveRestrictions)
REPLICA_ACCESS(int,CCharacter,m_PainSoundTimer)
REPLICA_ACCESS(int,CCharacter,m_NumInputs)
REPLICA_ACCESS(int,CCharacter,m_TuneZone)
REPLICA_ACCESS(int,CCharacter,m_TuneZoneOverride)
REPLICA_ACCESS(int,CCharacter,m_StrongWeakId)
REPLICA_ACCESS(int,CCharacter,m_LastWeaponSwitchTick)
REPLICA_ACCESS(int,CCharacter,m_LastTuneZoneTick)
REPLICA_ACCESS(CNetObj_PlayerInput,CCharacter,m_LatestPrevInput)
REPLICA_ACCESS(CNetObj_PlayerInput,CCharacter,m_LatestInput)
REPLICA_ACCESS(CNetObj_PlayerInput,CCharacter,m_PrevInput)
REPLICA_ACCESS(CNetObj_PlayerInput,CCharacter,m_Input)
REPLICA_ACCESS(CNetObj_PlayerInput,CCharacter,m_SavedInput)
REPLICA_ACCESS(int[MAX_CLIENTS],CCharacter,m_aHitObjects)
REPLICA_ACCESS(CCharacterCore,CCharacter,m_Core)
REPLICA_ACCESS(int,CCharacter,m_FreezeTime)
REPLICA_ACCESS(vec2,CCharacter,m_PrevPos)
REPLICA_ACCESS(CTuningParams*,CGameWorld,m_pTuningList)
REPLICA_ACCESS(bool,CCharacter,m_Interfering)
REPLICA_ACCESS(int,CCharacterCore,m_MoveRestrictions)
#undef REPLICA_ACCESS
struct WorldFlags {
bool m_IsDDRace{};
bool m_IsVanilla{};
bool m_IsFNG{};
bool m_InfiniteAmmo{};
bool m_PredictTiles{};
int m_PredictFreeze{};
bool m_PredictWeapons{};
bool m_PredictDDRace{};
bool m_IsSolo{};
bool m_UseTuneZones{};
bool m_BugDDRaceInput{};
bool m_NoWeakHookAndBounce{};
bool m_PredictEvents{};
bool m_OldLaser{};
bool m_PredictTeleports{};
bool operator==(const WorldFlags &) const = default;
};
inline void SetFlags(CGameWorld &World,const WorldFlags &Flags) {
World.m_WorldConfig.m_IsDDRace=Flags.m_IsDDRace;
World.m_WorldConfig.m_IsVanilla=Flags.m_IsVanilla;
World.m_WorldConfig.m_IsFNG=Flags.m_IsFNG;
World.m_WorldConfig.m_InfiniteAmmo=Flags.m_InfiniteAmmo;
World.m_WorldConfig.m_PredictTiles=Flags.m_PredictTiles;
World.m_WorldConfig.m_PredictFreeze=Flags.m_PredictFreeze;
World.m_WorldConfig.m_PredictWeapons=Flags.m_PredictWeapons;
World.m_WorldConfig.m_PredictDDRace=Flags.m_PredictDDRace;
World.m_WorldConfig.m_IsSolo=Flags.m_IsSolo;
World.m_WorldConfig.m_UseTuneZones=Flags.m_UseTuneZones;
World.m_WorldConfig.m_BugDDRaceInput=Flags.m_BugDDRaceInput;
World.m_WorldConfig.m_NoWeakHookAndBounce=Flags.m_NoWeakHookAndBounce;
World.m_WorldConfig.m_PredictEvents=Flags.m_PredictEvents;
World.m_WorldConfig.m_OldLaser=Flags.m_OldLaser;
World.m_WorldConfig.m_PredictTeleports=Flags.m_PredictTeleports;
// Own event generation is unnecessary and never changes game input or state.
World.m_WorldConfig.m_PredictEvents=false;
}
inline CCharacter *ImportCharacter(CGameWorld &World, std::span<const std::uint8_t> Bytes, int ClientId)
{
    Need(Bytes.size()==RemoteLayout::sizeof_CCharacter,"exact character length");
    Need(ClientId>=0 && ClientId<MAX_CLIENTS && !World.GetCharacterById(ClientId),"replica client id");
    Need(!Bool(Bytes,RemoteLayout::CEntity::m_MarkedForDestroy),"destroying entity");
    Need(Int(Bytes,RemoteLayout::CEntity::m_Id)==ClientId,"entity id");
    const auto CoreBytes=Bytes.subspan(RemoteLayout::CCharacter::m_Core,RemoteLayout::sizeof_CCharacterCore);
    Need(Int(CoreBytes,RemoteLayout::CCharacterCore::m_Id)==ClientId,"core id");
    // Pending native ABI check of this field: current single-character fixture
    // accepts ONLY an empty attached-player set. No remote tree is traversed.
    Need(U64(CoreBytes,RemoteLayout::CCharacterCore::m_AttachedPlayers+8)==0,"attached-player set unsupported");
    Need(Int(CoreBytes,RemoteLayout::CCharacterCore::m_HookedPlayer)==-1,"hooked player unsupported in this acquisition");
    const vec2 Position=Vector(CoreBytes,RemoteLayout::CCharacterCore::m_Pos);
    Need(Position==Vector(Bytes,RemoteLayout::CEntity::m_Pos),"entity/core position disagreement");
    CNetObj_Character Net{};Net.m_X=static_cast<int>(Position.x);Net.m_Y=static_cast<int>(Position.y);
    Net.m_HookedPlayer=-1;Net.m_Weapon=WEAPON_GUN;
    CNetObj_DDNetCharacter Extended{};
    CCharacter *Player=new CCharacter(&World,ClientId,&Net,&Extended);
    World.InsertEntity(Player); // From here World owns and destroys it on failure.
    auto &Core=Player->*Access(CCharacterm_Core{});
    Player->*Access(CCharacterm_NumObjectsHit{})=Int(Bytes,RemoteLayout::CCharacter::m_NumObjectsHit);
    Player->*Access(CCharacterm_LastWeapon{})=Int(Bytes,RemoteLayout::CCharacter::m_LastWeapon);
    Player->*Access(CCharacterm_QueuedWeapon{})=Int(Bytes,RemoteLayout::CCharacter::m_QueuedWeapon);
    Player->*Access(CCharacterm_ReloadTimer{})=Int(Bytes,RemoteLayout::CCharacter::m_ReloadTimer);
    Player->*Access(CCharacterm_AttackTick{})=Int(Bytes,RemoteLayout::CCharacter::m_AttackTick);
    Player->*Access(CCharacterm_MoveRestrictions{})=Int(Bytes,RemoteLayout::CCharacter::m_MoveRestrictions);
    Player->*Access(CCharacterm_PainSoundTimer{})=Int(Bytes,RemoteLayout::CCharacter::m_PainSoundTimer);
    Player->*Access(CCharacterm_NumInputs{})=Int(Bytes,RemoteLayout::CCharacter::m_NumInputs);
    Player->*Access(CCharacterm_TuneZone{})=Int(Bytes,RemoteLayout::CCharacter::m_TuneZone);
    Player->*Access(CCharacterm_TuneZoneOverride{})=Int(Bytes,RemoteLayout::CCharacter::m_TuneZoneOverride);
    Player->*Access(CCharacterm_StrongWeakId{})=Int(Bytes,RemoteLayout::CCharacter::m_StrongWeakId);
    Player->*Access(CCharacterm_LastWeaponSwitchTick{})=Int(Bytes,RemoteLayout::CCharacter::m_LastWeaponSwitchTick);
    Player->*Access(CCharacterm_LastTuneZoneTick{})=Int(Bytes,RemoteLayout::CCharacter::m_LastTuneZoneTick);
    Player->*Access(CCharacterm_LatestPrevInput{})=Input(Bytes,RemoteLayout::CCharacter::m_LatestPrevInput);
    Player->*Access(CCharacterm_LatestInput{})=Input(Bytes,RemoteLayout::CCharacter::m_LatestInput);
    Player->*Access(CCharacterm_PrevInput{})=Input(Bytes,RemoteLayout::CCharacter::m_PrevInput);
    Player->*Access(CCharacterm_Input{})=Input(Bytes,RemoteLayout::CCharacter::m_Input);
    Player->*Access(CCharacterm_SavedInput{})=Input(Bytes,RemoteLayout::CCharacter::m_SavedInput);
    Player->m_FreezeTime=Int(Bytes,RemoteLayout::CCharacter::m_FreezeTime);
    Player->m_TeleCheckpoint=Int(Bytes,RemoteLayout::CCharacter::m_TeleCheckpoint);
    Player->m_TileIndex=Int(Bytes,RemoteLayout::CCharacter::m_TileIndex);
    Player->m_TileFIndex=Int(Bytes,RemoteLayout::CCharacter::m_TileFIndex);
    Player->m_LastSnapWeapon=Int(Bytes,RemoteLayout::CCharacter::m_LastSnapWeapon);
    Player->m_GameTeam=Int(Bytes,RemoteLayout::CCharacter::m_GameTeam);
    Player->m_FreezeAccumulation=Int(Bytes,RemoteLayout::CCharacter::m_FreezeAccumulation);
    Player->m_AliveAccumulation=Int(Bytes,RemoteLayout::CCharacter::m_AliveAccumulation);
    Player->m_IsLocal=Bool(Bytes,RemoteLayout::CCharacter::m_IsLocal);
    Player->m_NinjaJetpack=Bool(Bytes,RemoteLayout::CCharacter::m_NinjaJetpack);
    Player->m_FrozenLastTick=Bool(Bytes,RemoteLayout::CCharacter::m_FrozenLastTick);
    Player->m_LastRefillJumps=Bool(Bytes,RemoteLayout::CCharacter::m_LastRefillJumps);
    Player->m_KeepHooked=Bool(Bytes,RemoteLayout::CCharacter::m_KeepHooked);
    Player->m_CanMoveInFreeze=Bool(Bytes,RemoteLayout::CCharacter::m_CanMoveInFreeze);
    Player->m_PrevPos=Vector(Bytes,RemoteLayout::CCharacter::m_PrevPos);
    Player->m_PrevPrevPos=Vector(Bytes,RemoteLayout::CCharacter::m_PrevPrevPos);
    Player->*Access(CCharacterm_Interfering{})=Bool(Bytes,RemoteLayout::CCharacter::m_Interfering);
    Need(Player->*Access(CCharacterm_NumObjectsHit{})>=0 && Player->*Access(CCharacterm_NumObjectsHit{})<=MAX_CLIENTS,"hit object count");
    Need(Player->*Access(CCharacterm_TuneZone{})==0 && Player->GetOverriddenTuneZone()==0,"tune-zone acquisition bound");
    for(int Id=0;Id<MAX_CLIENTS;++Id) (Player->*Access(CCharacterm_aHitObjects{}))[Id]=Int(Bytes,RemoteLayout::CCharacter::m_aHitObjects+static_cast<std::size_t>(Id)*4);
    Player->m_Pos=Position;
    Core.m_HookTick=Int(CoreBytes,RemoteLayout::CCharacterCore::m_HookTick);
    Core.m_HookState=Int(CoreBytes,RemoteLayout::CCharacterCore::m_HookState);
    Core.m_ActiveWeapon=Int(CoreBytes,RemoteLayout::CCharacterCore::m_ActiveWeapon);
    Core.m_Jumped=Int(CoreBytes,RemoteLayout::CCharacterCore::m_Jumped);
    Core.m_JumpedTotal=Int(CoreBytes,RemoteLayout::CCharacterCore::m_JumpedTotal);
    Core.m_Jumps=Int(CoreBytes,RemoteLayout::CCharacterCore::m_Jumps);
    Core.m_Direction=Int(CoreBytes,RemoteLayout::CCharacterCore::m_Direction);
    Core.m_Angle=Int(CoreBytes,RemoteLayout::CCharacterCore::m_Angle);
    Core.m_TriggeredEvents=Int(CoreBytes,RemoteLayout::CCharacterCore::m_TriggeredEvents);
    Core.m_Id=Int(CoreBytes,RemoteLayout::CCharacterCore::m_Id);
    Core.m_Colliding=Int(CoreBytes,RemoteLayout::CCharacterCore::m_Colliding);
    Core.m_FreezeStart=Int(CoreBytes,RemoteLayout::CCharacterCore::m_FreezeStart);
    Core.m_FreezeEnd=Int(CoreBytes,RemoteLayout::CCharacterCore::m_FreezeEnd);
    Core.m_NewHook=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_NewHook);
    Core.m_Reset=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_Reset);
    Core.m_LeftWall=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_LeftWall);
    Core.m_Solo=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_Solo);
    World.Teams()->SetSolo(ClientId,Core.m_Solo);
    Core.m_Jetpack=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_Jetpack);
    Core.m_CollisionDisabled=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_CollisionDisabled);
    Core.m_EndlessHook=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_EndlessHook);
    Core.m_EndlessJump=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_EndlessJump);
    Core.m_HammerHitDisabled=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_HammerHitDisabled);
    Core.m_GrenadeHitDisabled=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_GrenadeHitDisabled);
    Core.m_LaserHitDisabled=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_LaserHitDisabled);
    Core.m_ShotgunHitDisabled=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_ShotgunHitDisabled);
    Core.m_HookHitDisabled=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_HookHitDisabled);
    Core.m_Super=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_Super);
    Core.m_Invincible=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_Invincible);
    Core.m_HasTelegunGun=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_HasTelegunGun);
    Core.m_HasTelegunGrenade=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_HasTelegunGrenade);
    Core.m_HasTelegunLaser=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_HasTelegunLaser);
    Core.m_IsInFreeze=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_IsInFreeze);
    Core.m_DeepFrozen=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_DeepFrozen);
    Core.m_LiveFrozen=Bool(CoreBytes,RemoteLayout::CCharacterCore::m_LiveFrozen);
    Core.m_Pos=Vector(CoreBytes,RemoteLayout::CCharacterCore::m_Pos);
    Core.m_Vel=Vector(CoreBytes,RemoteLayout::CCharacterCore::m_Vel);
    Core.m_HookPos=Vector(CoreBytes,RemoteLayout::CCharacterCore::m_HookPos);
    Core.m_HookDir=Vector(CoreBytes,RemoteLayout::CCharacterCore::m_HookDir);
    Core.m_HookTeleBase=Vector(CoreBytes,RemoteLayout::CCharacterCore::m_HookTeleBase);
    Core.m_Input=Input(CoreBytes,RemoteLayout::CCharacterCore::m_Input);
    Core.*Access(CCharacterCorem_MoveRestrictions{})=Int(CoreBytes,RemoteLayout::CCharacterCore::m_MoveRestrictions);
    static_assert(sizeof(CCharacterCore::CWeaponStat)==16);
    // Candidate nested offsets are compile-asserted, then checked with MSVC in
    // the native fixture before this importer can be used for ordinary input.
    static_assert(offsetof(CCharacterCore::CWeaponStat,m_AmmoRegenStart)==0 && offsetof(CCharacterCore::CWeaponStat,m_Ammo)==4 && offsetof(CCharacterCore::CWeaponStat,m_Ammocost)==8 && offsetof(CCharacterCore::CWeaponStat,m_Got)==12);
    for(int Weapon=0;Weapon<NUM_WEAPONS;++Weapon)
    {
        const auto At=RemoteLayout::CCharacterCore::m_aWeapons+static_cast<std::size_t>(Weapon)*16;
        Core.m_aWeapons[Weapon].m_AmmoRegenStart=Int(CoreBytes,At);
        Core.m_aWeapons[Weapon].m_Ammo=Int(CoreBytes,At+4);
        Core.m_aWeapons[Weapon].m_Ammocost=Int(CoreBytes,At+8);
        Core.m_aWeapons[Weapon].m_Got=Bool(CoreBytes,At+12);
    }
    Core.m_Ninja.m_ActivationDir=Vector(CoreBytes,RemoteLayout::CCharacterCore::m_Ninja);
    Core.m_Ninja.m_ActivationTick=Int(CoreBytes,RemoteLayout::CCharacterCore::m_Ninja+8);
    Core.m_Ninja.m_CurrentMoveTime=Int(CoreBytes,RemoteLayout::CCharacterCore::m_Ninja+12);
    Core.m_Ninja.m_OldVelAmount=Int(CoreBytes,RemoteLayout::CCharacterCore::m_Ninja+16);
    for(int Param=0;Param<CTuningParams::Num();++Param)
    {
        const int Value=Int(CoreBytes,RemoteLayout::CCharacterCore::m_Tuning+static_cast<std::size_t>(Param)*4);
        Need(Value>-100000000 && Value<100000000,"excessive tuning");
        Core.m_Tuning.NetworkArray()[Param]=Value;
    }
    World.TuningList()[0]=Core.m_Tuning;
    Player->SetCoreWorld(&World); // Fresh OWN pointers/callback; no remote pointer escapes.
    return Player;
}
inline void Step(CGameWorld &World, int ClientId, const CNetObj_PlayerInput &NewInput)
{
    Need(World.m_GameTick>=0 && World.m_GameTick<std::numeric_limits<int>::max(),"world tick range");
    auto *Player=World.GetCharacterById(ClientId);Need(Player!=nullptr,"missing character before direct input");
    Player->OnDirectInput(&NewInput);
    ++World.m_GameTick;
    Player=World.GetCharacterById(ClientId);Need(Player!=nullptr,"character removed by direct input");
    Player->OnPredictedInput(&NewInput);
    World.Tick();
    Need(World.GetCharacterById(ClientId)!=nullptr,"character removed during tick");
}
} // namespace Replica
