// SPDX-License-Identifier: MIT
#include "policy.h"
#include "config_fields.h"
#include "physics_owned_map.h"
#include <engine/shared/config.h>
#include <game/mapbugs.h>
#include <algorithm>
#include <chrono>
#include <cstring>

namespace malinka::avoid
{
namespace
{
using Clock=std::chrono::steady_clock;
void ApplyConfig(const Frame &frame)
{
    std::size_t i=0;
#define COPY_CONFIG(Name) g_Config.m_##Name=frame.config[i++];
    MALINKA_PREDICTION_CONFIG(COPY_CONFIG)
#undef COPY_CONFIG
    Replica::Need(i==CONFIG_COUNT,"configuration schema");
}
bool SupportedTileState(int tile)
{
    switch(tile)
    {
    case TILE_AIR:case TILE_SOLID:case TILE_DEATH:case TILE_NOHOOK:case TILE_FREEZE:case TILE_UNFREEZE:
    case TILE_DFREEZE:case TILE_DUNFREEZE:case TILE_LFREEZE:case TILE_LUNFREEZE:case TILE_THROUGH:
    case TILE_THROUGH_ALL:case TILE_THROUGH_CUT:case TILE_THROUGH_DIR:case TILE_STOP:case TILE_STOPS:
    case TILE_STOPA:case TILE_WALLJUMP:case TILE_REFILL_JUMPS:case TILE_UNLIMITED_JUMPS_ENABLE:
    case TILE_UNLIMITED_JUMPS_DISABLE:case TILE_START:case TILE_FINISH:
        return true;
    default:return false;
    }
}
const StaticMap &GuardFrame(const Frame &frame)
{
    namespace R=RemoteLayout;
    Replica::Need(frame.map!=nullptr && frame.character.size()==R::sizeof_CCharacter,"complete frame required");
    constexpr int TICK_MARGIN=2000000;
    Replica::Need(frame.client_id>=0 && frame.client_id<MAX_CLIENTS && frame.dummy>=0 && frame.dummy<2 && frame.tick>=0 && frame.tick<std::numeric_limits<int>::max()-TICK_MARGIN,"frame identity/tick range");
    Replica::Need(frame.flags.m_IsDDRace && frame.flags.m_PredictDDRace && frame.flags.m_PredictTiles && frame.flags.m_PredictFreeze>=0 && frame.flags.m_PredictFreeze<=2,"unsupported prediction mode");
    Replica::Need(frame.map->width>0 && frame.map->height>0 && static_cast<std::uint64_t>(frame.map->width)*static_cast<std::uint64_t>(frame.map->height)<=MAX_PREVIEW_TILES,"map dimension budget");
    const auto map_cells=static_cast<std::size_t>(frame.map->width)*static_cast<std::size_t>(frame.map->height);
    const auto expected_map_bytes=map_cells*sizeof(CTile);
    Replica::Need(frame.map->game.size()==expected_map_bytes && (frame.map->front.empty() || frame.map->front.size()==expected_map_bytes),"incomplete static map layers");
    for(const auto *layer : {&frame.map->game,&frame.map->front})
        for(std::size_t at=0;at<layer->size();at+=sizeof(CTile))
            Replica::Need(SupportedStaticMapTile((*layer)[at]),"unsupported static map tile");
    std::size_t config_index=0;
    // Bounds correspond to these exact pinned config declarations. A zero
    // maximum in other game declarations is NOT treated as a real zero value.
    constexpr std::array<int,CONFIG_COUNT> minima{0,0,0,0,1,1,0,0,0,0,0,5,0};
    constexpr std::array<int,CONFIG_COUNT> maxima{3,1,1,1,99999,30,1,1,1,3,1,150,2};
    for(int value : frame.config){Replica::Need(value>=minima[config_index] && value<=maxima[config_index],"prediction config bound");++config_index;}
    Replica::Need(config_index==CONFIG_COUNT,"configuration schema");
    const auto bytes=std::span(frame.character),core=bytes.subspan(R::CCharacter::m_Core,R::sizeof_CCharacterCore);
    auto integer_bound=[](int value,int lower,int upper){Replica::Need(value>=lower && value<=upper,"character scalar/index bound");};
    auto in_map=[&frame](vec2 position)
    {
        return std::isfinite(position.x) && std::isfinite(position.y) && position.x>=-64 && position.y>=-64 &&
            position.x<=static_cast<float>(frame.map->width*32+64) && position.y<=static_cast<float>(frame.map->height*32+64);
    };
    Replica::Need(!Replica::Bool(bytes,R::CEntity::m_MarkedForDestroy),"destroying entity");
    Replica::Need(Replica::Int(bytes,R::CEntity::m_ObjType)==CGameWorld::ENTTYPE_CHARACTER && Replica::Float(bytes,R::CEntity::m_ProximityRadius)==CCharacterCore::PhysicalSize(),"character type/radius disagreement");
    const auto position=Replica::Vector(core,R::CCharacterCore::m_Pos);
    Replica::Need(position==Replica::Vector(bytes,R::CEntity::m_Pos) && in_map(position),"character position/map disagreement");
    Replica::Need(in_map(Replica::Vector(bytes,R::CCharacter::m_PrevPos)) && in_map(Replica::Vector(bytes,R::CCharacter::m_PrevPrevPos)),"previous position/map bound");
    Replica::Need(SupportedTileState(Replica::Int(bytes,R::CCharacter::m_TileIndex)) && SupportedTileState(Replica::Int(bytes,R::CCharacter::m_TileFIndex)),"unsupported cached character tile");
    integer_bound(Replica::Int(bytes,R::CCharacter::m_TeleCheckpoint),0,0);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_GameTeam),-1,MAX_CLIENTS);
    for(auto field : {R::CCharacter::m_QueuedWeapon,R::CCharacter::m_LastWeapon,R::CCharacter::m_LastSnapWeapon})integer_bound(Replica::Int(bytes,field),-1,NUM_WEAPONS-1);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_QueuedWeapon),-1,-1);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_ActiveWeapon),0,NUM_WEAPONS-1);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_NumInputs),0,std::numeric_limits<int>::max()-TICK_MARGIN);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_NumObjectsHit),0,MAX_CLIENTS);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_ReloadTimer),0,1000000);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_FreezeTime),0,1000000);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_AttackTick),-1000000,frame.tick+1000);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_LastWeaponSwitchTick),-1000000,frame.tick+1000);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_LastTuneZoneTick),-1000000,frame.tick+1000);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_PainSoundTimer),-1000000,1000000);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_FreezeAccumulation),0,frame.config[11]);
    integer_bound(Replica::Int(bytes,R::CCharacter::m_AliveAccumulation),-frame.config[11],frame.config[11]);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_HookState),-1,5);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_HookTick),-1000000,frame.tick+1000);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_Jumped),0,3);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_Jumps),-1,100000);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_JumpedTotal),0,100000);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_Direction),-1,1);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_Angle),0,2048);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_TriggeredEvents),0,0x7f);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_Colliding),0,2);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_FreezeStart),-1000000,frame.tick+1000);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_FreezeEnd),-1,frame.tick+1000000);
    const int character_restrictions=Replica::Int(bytes,R::CCharacter::m_MoveRestrictions);
    const int core_restrictions=Replica::Int(core,R::CCharacterCore::m_MoveRestrictions);
    Replica::Need((character_restrictions&~15)==0 && (core_restrictions&~15)==0,"movement restriction mask");
    for(std::size_t i=0;i<static_cast<std::size_t>(Replica::Int(bytes,R::CCharacter::m_NumObjectsHit));++i)
        integer_bound(Replica::Int(bytes,R::CCharacter::m_aHitObjects+i*4),0,MAX_CLIENTS-1);
    auto input_bound=[&integer_bound](const CNetObj_PlayerInput &input)
    {
        integer_bound(input.m_Direction,-1,1);integer_bound(input.m_TargetX,-999999,999999);integer_bound(input.m_TargetY,-999999,999999);
        integer_bound(input.m_Jump,0,1);integer_bound(input.m_Hook,0,1);
        integer_bound(input.m_Fire,0,INPUT_STATE_MASK);integer_bound(input.m_NextWeapon,0,INPUT_STATE_MASK);integer_bound(input.m_PrevWeapon,0,INPUT_STATE_MASK);integer_bound(input.m_WantedWeapon,0,NUM_WEAPONS);
        integer_bound(input.m_PlayerFlags,0,65535);
    };
    input_bound(frame.controls);
    std::array<CNetObj_PlayerInput,5> stored_inputs{};
    constexpr std::array<std::size_t,5> input_offsets{R::CCharacter::m_LatestInput,R::CCharacter::m_LatestPrevInput,R::CCharacter::m_PrevInput,R::CCharacter::m_Input,R::CCharacter::m_SavedInput};
    for(std::size_t i=0;i<input_offsets.size();++i)
    {
        stored_inputs[i]=Replica::Input(bytes,input_offsets[i]);input_bound(stored_inputs[i]);
        Replica::Need((stored_inputs[i].m_Fire&1)==0 && stored_inputs[i].m_WantedWeapon==0,"held/unmodelled weapon input");
    }
    const auto &latest=stored_inputs[0],&latest_previous=stored_inputs[1];
    Replica::Need(CountInput(latest_previous.m_Fire,latest.m_Fire).m_Presses==0 &&
        CountInput(latest_previous.m_NextWeapon,latest.m_NextWeapon).m_Presses==0 &&
        CountInput(latest_previous.m_PrevWeapon,latest.m_PrevWeapon).m_Presses==0,"pending fire/weapon input event");
    Replica::Need(CountInput(latest.m_Fire,frame.controls.m_Fire).m_Presses==0 &&
        CountInput(latest.m_NextWeapon,frame.controls.m_NextWeapon).m_Presses==0 &&
        CountInput(latest.m_PrevWeapon,frame.controls.m_PrevWeapon).m_Presses==0 &&
        frame.controls.m_WantedWeapon==0 && (frame.controls.m_Fire&1)==0,"unobserved fire/weapon input event");
    CTuningParams tuning;
    for(int parameter=0;parameter<CTuningParams::Num();++parameter)
    {
        const int value=Replica::Int(core,R::CCharacterCore::m_Tuning+static_cast<std::size_t>(parameter)*4);
        integer_bound(value,-1000000,1000000);tuning.NetworkArray()[parameter]=value;
    }
    auto tuning_bound=[](float value,float lower,float upper){Replica::Need(value>=lower && value<=upper,"preview tuning bound");};
    tuning_bound(tuning.m_Gravity,-16,16);tuning_bound(tuning.m_GroundControlSpeed,0,128);tuning_bound(tuning.m_GroundControlAccel,0,128);
    tuning_bound(tuning.m_GroundFriction,0,2);tuning_bound(tuning.m_GroundJumpImpulse,0,128);tuning_bound(tuning.m_AirControlSpeed,0,128);
    tuning_bound(tuning.m_AirControlAccel,0,128);tuning_bound(tuning.m_AirFriction,0,2);tuning_bound(tuning.m_AirJumpImpulse,0,128);
    tuning_bound(tuning.m_HookLength,0,8192);tuning_bound(tuning.m_HookFireSpeed,0,1024);tuning_bound(tuning.m_HookDragAccel,0,128);
    tuning_bound(tuning.m_HookDragSpeed,0,128);tuning_bound(tuning.m_HookDuration,0,60);tuning_bound(tuning.m_VelrampStart,0,10000);
    tuning_bound(tuning.m_VelrampRange,1,10000);tuning_bound(tuning.m_VelrampCurvature,1,10);
    tuning_bound(tuning.m_GroundElasticityX,-16,16);tuning_bound(tuning.m_GroundElasticityY,-16,16);
    Replica::Need((tuning.m_PlayerCollision==0 || tuning.m_PlayerCollision==1) && (tuning.m_PlayerHooking==0 || tuning.m_PlayerHooking==1),"player collision tuning bound");
    integer_bound(Replica::Int(core,R::CCharacterCore::m_aWeapons+WEAPON_NINJA*16+4),-1,1000000);
    Replica::Need(Replica::Int(core,R::CCharacterCore::m_ActiveWeapon)!=WEAPON_NINJA &&
        !Replica::Bool(core,R::CCharacterCore::m_aWeapons+WEAPON_NINJA*16+12),"ninja weapon prediction is not acquired");
    for(int weapon=0;weapon<NUM_WEAPONS;++weapon)
    {
        const auto offset=R::CCharacterCore::m_aWeapons+static_cast<std::size_t>(weapon)*16;
        integer_bound(Replica::Int(core,offset),-1000000,frame.tick+1000);
        integer_bound(Replica::Int(core,offset+4),-1,1000000);
        integer_bound(Replica::Int(core,offset+8),0,1000);
        (void)Replica::Bool(core,offset+12);
    }
    const auto ninja_direction=Replica::Vector(core,R::CCharacterCore::m_Ninja);
    Replica::Need(std::abs(ninja_direction.x)<=2 && std::abs(ninja_direction.y)<=2,"ninja direction bound");
    integer_bound(Replica::Int(core,R::CCharacterCore::m_Ninja+8),-1000000,frame.tick+1000);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_Ninja+12),0,1000);
    integer_bound(Replica::Int(core,R::CCharacterCore::m_Ninja+16),0,6000);
    for(auto field : {R::CCharacterCore::m_HookPos,R::CCharacterCore::m_HookTeleBase})Replica::Need(in_map(Replica::Vector(core,field)),"hook position/map bound");
    const auto hook_direction=Replica::Vector(core,R::CCharacterCore::m_HookDir);
    Replica::Need(std::abs(hook_direction.x)<=2 && std::abs(hook_direction.y)<=2,"hook direction bound");
    return *frame.map;
}
struct OwnedWorld
{
    CPhysicsOwnedMap map;
    CLayers layers;
    CCollision collision;
    std::array<CTuningParams,256> tuning{};
    CMapBugs bugs;
    CGameWorld world;
    explicit OwnedWorld(const Frame &frame) : OwnedWorld(frame,GuardFrame(frame)) {}
    OwnedWorld(const Frame &frame,const StaticMap &source) : map(source.width,source.height,source.game)
    {
        if(!frame.map->front.empty())map.AddLayer(TILESLAYERFLAG_FRONT,frame.map->front);
        layers.Init(&map,false,false);collision.Init(&layers);
        ApplyConfig(frame);world.Teams()->Reset();
        world.Init(&collision,tuning.data(),&bugs);world.m_GameTick=frame.tick;world.m_LocalClientId=frame.client_id;
        Replica::SetFlags(world,frame.flags);
        Replica::ImportCharacter(world,frame.character,frame.client_id);
    }
};
bool HazardTile(int tile)
{
    return tile==TILE_FREEZE || tile==TILE_DFREEZE || tile==TILE_LFREEZE || tile==TILE_DEATH;
}
bool Hazard(CCollision &collision,vec2 position)
{
    const int index=collision.GetPureMapIndex(position);
    if(HazardTile(collision.GetTileIndex(index)) || HazardTile(collision.GetFrontTileIndex(index)))return true;
    const float offset=CCharacterCore::PhysicalSize()/3.0f;
    for(float x : {-offset,offset})for(float y : {-offset,offset})
    {
        const int corner=collision.GetPureMapIndex(position+vec2(x,y));
        if(collision.GetTileIndex(corner)==TILE_DEATH || collision.GetFrontTileIndex(corner)==TILE_DEATH)return true;
    }
    return false;
}
void BoundedCore(const CCharacterCore &core,const StaticMap &map)
{
    Replica::Need(std::isfinite(core.m_Pos.x) && std::isfinite(core.m_Pos.y) && std::isfinite(core.m_Vel.x) && std::isfinite(core.m_Vel.y),"nonfinite simulation state");
    // Defensive preview limits, not recovered KRX constants or game limits.
    Replica::Need(std::abs(core.m_Vel.x)<=1024 && std::abs(core.m_Vel.y)<=1024,"preview velocity bound");
    Replica::Need(core.m_Pos.x>=-64 && core.m_Pos.y>=-64 && core.m_Pos.x<=static_cast<float>(map.width*32+64) && core.m_Pos.y<=static_cast<float>(map.height*32+64),"preview map-position bound");
}
int Evaluate(OwnedWorld &base,const Frame &frame,int direction,Clock::time_point deadline)
{
    // Both worlds and collision are OWNED by this companion. CopyWorld never
    // receives a game-process pointer; only remapped replicas exist here.
    CGameWorld world;world.CopyWorld(&base.world);world.m_WorldConfig.m_PredictEvents=false;
    for(int step=1;step<=HORIZON_TICKS;++step)
    {
        Replica::Need(Clock::now()<deadline,"prediction time budget");
        auto *player=world.GetCharacterById(frame.client_id);Replica::Need(player!=nullptr,"missing simulated character");
        const auto &before=*player->Core();BoundedCore(before,*frame.map);const vec2 position=before.m_Pos;
        if(player->m_FreezeTime>0 || before.m_DeepFrozen || before.m_LiveFrozen || Hazard(base.collision,position))return step;
        auto input=frame.controls;
        // Reaction/dispatch is not assumed instantaneous. Delay compensation
        // is conservative and intentionally does not modify the client input.
        if(step>INPUT_DELAY_TICKS)input.m_Direction=direction;
        Replica::Step(world,frame.client_id,input);
        player=world.GetCharacterById(frame.client_id);Replica::Need(player!=nullptr,"simulated character removed");
        const auto &after=*player->Core();BoundedCore(after,*frame.map);
        if(player->m_FreezeTime>0 || after.m_DeepFrozen || after.m_LiveFrozen || Hazard(base.collision,after.m_Pos))return step;
        const auto indices=base.collision.GetMapIndices(position,after.m_Pos,512);
        Replica::Need(indices.size()<512,"swept tile count budget");
        for(int index : indices)if(HazardTile(base.collision.GetTileIndex(index)) || HazardTile(base.collision.GetFrontTileIndex(index)))return step;
    }
    return 0;
}
} // namespace
bool SupportedStaticMapTile(int tile)
{
    return SupportedTileState(tile) || tile==ENTITY_OFFSET+ENTITY_SPAWN || tile==ENTITY_OFFSET+ENTITY_SPAWN_RED || tile==ENTITY_OFFSET+ENTITY_SPAWN_BLUE;
}
Transition CheckTransition(const Frame &before,const Frame &after)
{
    if(!before.map || !after.map || before.map->generation!=after.map->generation || before.client_id!=after.client_id || before.dummy!=after.dummy ||
        before.config!=after.config || before.flags!=after.flags || before.character.size()!=RemoteLayout::sizeof_CCharacter || after.character.size()!=RemoteLayout::sizeof_CCharacter)return Transition::learning;
    // Controls may already contain a new OS event that was not yet consumed
    // by prediction. Compare using the measured saved input actually applied
    // to these characters, not a guessed input-delivery tick.
    const auto applied_before=Replica::Input(before.character,RemoteLayout::CCharacter::m_SavedInput);
    const auto applied_after=Replica::Input(after.character,RemoteLayout::CCharacter::m_SavedInput);
    if(std::memcmp(&applied_before,&applied_after,sizeof(applied_before))!=0)return Transition::learning;
    const auto gap=static_cast<std::int64_t>(after.tick)-before.tick;
    if(gap<1 || gap>8)return Transition::learning;
    try
    {
        OwnedWorld own(before);
        for(std::int64_t step=0;step<gap;++step)Replica::Step(own.world,before.client_id,applied_before);
        const auto *player=own.world.GetCharacterById(before.client_id);Replica::Need(player!=nullptr,"transition character disappeared");
        const auto expected=std::span(after.character).subspan(RemoteLayout::CCharacter::m_Core,RemoteLayout::sizeof_CCharacterCore);
        const auto &core=*player->Core();
        return core.m_Pos==Replica::Vector(expected,RemoteLayout::CCharacterCore::m_Pos) && core.m_Vel==Replica::Vector(expected,RemoteLayout::CCharacterCore::m_Vel) &&
            core.m_IsInFreeze==Replica::Bool(expected,RemoteLayout::CCharacterCore::m_IsInFreeze) &&
            player->m_FreezeTime==Replica::Int(after.character,RemoteLayout::CCharacter::m_FreezeTime) ? Transition::matched : Transition::mismatch;
    }
    catch(const std::exception &) {return Transition::mismatch;}
}
Decision Choose(const Frame &frame,int physical_direction,const std::array<bool,3> &allowed)
{
    Decision result;result.direction=physical_direction;
    try
    {
        Replica::Need(frame.map!=nullptr && physical_direction>=-1 && physical_direction<=1,"missing map or input range");
        const auto deadline=Clock::now()+std::chrono::milliseconds(8);
        OwnedWorld base(frame);
        const auto *player=base.world.GetCharacterById(frame.client_id);Replica::Need(player!=nullptr,"no local replica");const auto &core=*player->Core();
        if(player->m_FreezeTime>0 || core.m_DeepFrozen || core.m_LiveFrozen)
        {result.state=DecisionState::already_frozen;result.reason="already frozen; no corrective key held";return result;}
        if(core.m_Super || core.m_Invincible)
        {result.state=DecisionState::no_danger;result.reason="freeze immunity; correction unnecessary";return result;}
        BoundedCore(core,*frame.map);
        result.baseline_danger_tick=Evaluate(base,frame,physical_direction,deadline);
        if(result.baseline_danger_tick==0)
        {result.state=DecisionState::no_danger;result.reason="current physical direction has no predicted hazard";return result;}
        // Least intrusive candidate first: stop, then left/right. Never choose
        // an unavailable prediction or an input the host cannot safely realize.
        for(int direction : {0,-1,1})
        {
            if(direction==physical_direction || !allowed[static_cast<std::size_t>(direction+1)])continue;
            if(Evaluate(base,frame,direction,deadline)==0)
            {result.state=DecisionState::correcting;result.direction=direction;result.reason="bounded independent prediction found a safe direction";return result;}
        }
        result.state=DecisionState::no_safe_correction;result.reason="no safe realizable correction in the 40-tick horizon";
    }
    catch(const std::exception &error){result.state=DecisionState::unavailable;result.reason=error.what();}
    return result;
}
} // namespace malinka::avoid
