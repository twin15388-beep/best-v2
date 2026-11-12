// SPDX-License-Identifier: MIT
#include "live_win32.h"
#include "config_fields.h"
// windows.h macros collide with enum members in the pinned engine headers.
#ifdef ERROR
#undef ERROR
#endif
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif
#ifdef RELATIVE
#undef RELATIVE
#endif
#ifdef IGNORE
#undef IGNORE
#endif
#include <engine/client.h>
#include <engine/keys.h>
#include <game/client/gameclient.h>
#include <engine/shared/config.h>
#include <algorithm>
#include <cstring>
#include <set>

namespace Replica
{
struct CGameClientm_aTuningList
{
    using Type=CTuningParams (CGameClient::*)[TuneZone::NUM];
    friend constexpr Type Access(CGameClientm_aTuningList);
};
template struct PrivateField<CGameClientm_aTuningList,&CGameClient::m_aTuningList>;
} // namespace Replica

namespace malinka::avoid
{
namespace
{
std::uint64_t CreationTime(HANDLE process)
{
    FILETIME creation{},exit{},kernel{},user{};
    Replica::Need(GetProcessTimes(process,&creation,&exit,&kernel,&user)!=FALSE,"process creation-time check");
    return std::uint64_t(creation.dwLowDateTime)|(std::uint64_t(creation.dwHighDateTime)<<32);
}
bool SameCorePrimitives(std::span<const std::uint8_t> first,std::span<const std::uint8_t> second)
{
    namespace C=RemoteLayout::CCharacterCore;
    for(auto field : {C::m_Pos,C::m_Vel,C::m_HookPos,C::m_HookDir,C::m_HookTeleBase})
        if(Replica::Vector(first,field)!=Replica::Vector(second,field))return false;
    for(auto field : {C::m_HookTick,C::m_HookState,C::m_Jumped,C::m_JumpedTotal,C::m_Jumps,C::m_Direction,C::m_Angle,C::m_Id,C::m_FreezeStart,C::m_FreezeEnd})
        if(Replica::Int(first,field)!=Replica::Int(second,field))return false;
    for(auto field : {C::m_IsInFreeze,C::m_DeepFrozen,C::m_LiveFrozen,C::m_Solo,C::m_Super,C::m_Invincible,C::m_EndlessHook,C::m_EndlessJump})
        if(Replica::Bool(first,field)!=Replica::Bool(second,field))return false;
    return std::equal(first.begin()+static_cast<std::ptrdiff_t>(C::m_Input),first.begin()+static_cast<std::ptrdiff_t>(C::m_Input+40),second.begin()+static_cast<std::ptrdiff_t>(C::m_Input)) &&
        std::equal(first.begin()+static_cast<std::ptrdiff_t>(C::m_Tuning),first.begin()+static_cast<std::ptrdiff_t>(C::m_Tuning+188),second.begin()+static_cast<std::ptrdiff_t>(C::m_Tuning));
}
template<class PointerToMember> std::size_t NativeMemberOffset(PointerToMember member)
{
    // MSVC's complete, single-inheritance data-member-pointer ABI. This is
    // measured in our compiled public types, never read as a target pointer.
    static_assert(sizeof(PointerToMember)==sizeof(std::int32_t));
    std::int32_t offset{};std::memcpy(&offset,&member,sizeof(offset));
    Replica::Need(offset>=0,"native member-pointer representation");return static_cast<std::size_t>(offset);
}
} // namespace
void ValidateNativeReplicaAbi()
{
    static_assert(sizeof(::CCharacter)==RemoteLayout::sizeof_CCharacter);
    static_assert(sizeof(::CCharacterCore)==RemoteLayout::sizeof_CCharacterCore);
    static_assert(sizeof(::CConfig)==RemoteLayout::sizeof_CConfig);
    static_assert(sizeof(::CNetObj_PlayerInput)==40 && sizeof(::CTuningParams)==188);
#define ASSERT_CONFIG(Name) static_assert(offsetof(::CConfig,m_##Name)==RemoteLayout::CConfig::m_##Name);
    MALINKA_PREDICTION_CONFIG(ASSERT_CONFIG)
#undef ASSERT_CONFIG
    Replica::Need(NativeMemberOffset(Access(Replica::CGameClientm_aTuningList{}))==RemoteLayout::CGameClient::m_aTuningList,"native effective tuning array offset");
    Replica::Need(NativeMemberOffset(Access(Replica::CCharacterm_FreezeTime{}))==RemoteLayout::CCharacter::m_FreezeTime,"native freeze-time member offset");
    Replica::Need(NativeMemberOffset(Access(Replica::CCharacterm_PrevPos{}))==RemoteLayout::CCharacter::m_PrevPos,"native previous-position member offset");
    Replica::Need(NativeMemberOffset(Access(Replica::CCharacterm_Core{}))==RemoteLayout::CCharacter::m_Core,"native core member offset");
    Replica::Need(NativeMemberOffset(Access(Replica::CGameWorldm_pTuningList{}))==RemoteLayout::CGameWorld::m_pTuningList,"native tuning pointer member offset");
    std::set<int> values;
    static_assert(sizeof(values)==16);
    for(int size : {0,1,3})
    {
        while(values.size()<static_cast<std::size_t>(size))values.insert(static_cast<int>(values.size()));
        const auto bytes=std::as_bytes(std::span{&values,1});
        std::uint64_t measured{};std::memcpy(&measured,bytes.data()+8,sizeof(measured));
        Replica::Need(measured==values.size(),"native attached-set count layout");
    }
}
LiveReader::LiveReader(const helper::Report &report)
{
    ValidateNativeReplicaAbi();
    Replica::Need(report.file.recognized_public_release && report.file.vtable_file_verified && report.process.has_value(),"exact official image required");
    const auto &process=*report.process;
    Replica::Need(process.error.empty() && process.read_only_handle && process.loaded_headers_match && process.loaded_vtable_matches &&
        process.module_size==helper::PUBLIC_311.image_size && process.scan.status=="complete" && process.scan.candidates.size()==1,"a complete unique root inspection is required");
    pid_=process.pid;creation_=process.creation_time_100ns;module_=process.module_base;root_=process.scan.candidates.front();
    Replica::Need(helper::UserRange(root_,11064576),"CGameClient allocation range");
    process_.Reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ|SYNCHRONIZE,FALSE,pid_));
    Replica::Need(bool(process_) && CreationTime(process_.Get())==creation_ && Alive(),"read-only process identity changed");
}
bool LiveReader::Alive() const {return WaitForSingleObject(process_.Get(),0)==WAIT_TIMEOUT;}
std::vector<std::uint8_t> LiveReader::Read(std::uint64_t address,std::size_t count) const
{
    Replica::Need(count>0 && count<=1024*1024,"read count bound");
    std::vector<std::uint8_t> bytes(count);
    Replica::Need(helper::ReadExact(process_.Get(),address,std::as_writable_bytes(std::span(bytes))),"read failed or partial");return bytes;
}
int LiveReader::Int(std::uint64_t address) const {return Replica::Int(Read(address,4),0);}
std::uint64_t LiveReader::Pointer(std::uint64_t address) const {return Replica::U64(Read(address,8),0);}
bool LiveReader::ExactBinding(int key,std::string_view command) const
{
    if(key<0 || key>=KEY_LAST)return false;
    const auto pointer=Pointer(root_+RemoteLayout::CGameClient::m_Binds+RemoteLayout::CBinds::m_aapKeyBindings+static_cast<std::size_t>(key)*8);
    if(!helper::UserRange(pointer,command.size()+1))return false;
    const auto bytes=Read(pointer,command.size()+1);
    return bytes.back()==0 && std::equal(command.begin(),command.end(),bytes.begin(),[](char a,std::uint8_t b){return static_cast<unsigned char>(a)==b;});
}
bool LiveReader::ArrowLeftBound() const {return ExactBinding(KEY_LEFT,"+left");}
bool LiveReader::ArrowRightBound() const {return ExactBinding(KEY_RIGHT,"+right");}
std::optional<Frame> LiveReader::Capture()
{
    namespace R=RemoteLayout;
    Frame frame;frame.measured_ms=helper::MonotonicMs();
    Replica::Need(Alive() && CreationTime(process_.Get())==creation_,"target exited or identity changed");
    Replica::Need(Pointer(root_)==module_+helper::PUBLIC_311.vtable_rva,"root vtable changed");
    const auto table=Read(module_+helper::PUBLIC_311.vtable_rva-8,72);
    Replica::Need(Replica::U64(table,0)==module_+helper::PUBLIC_311.locator_rva,"loaded RTTI fingerprint changed");
    for(std::size_t i=0;i<helper::PUBLIC_311.first_function_rvas.size();++i)
        Replica::Need(Replica::U64(table,(i+1)*8)==module_+helper::PUBLIC_311.first_function_rvas[i],"loaded vtable fingerprint changed");
    const auto engine=Pointer(root_+R::CGameClient::m_pClient),config=Pointer(root_+R::CGameClient::m_pConfig);
    Replica::Need(helper::UserRange(engine,8000) && helper::UserRange(config,R::sizeof_CConfig),"engine/config not initialized");
    Replica::Need(Int(engine+R::IClient::m_State)==IClient::STATE_ONLINE,"client is not online");
    frame.dummy=Int(config+R::CConfig::m_ClDummy);Replica::Need(frame.dummy>=0 && frame.dummy<2,"selected dummy range");
    const auto slot=static_cast<std::size_t>(frame.dummy);
    frame.tick=Int(engine+R::IClient::m_aPredTick+slot*4);Replica::Need(frame.tick>=0,"prediction tick range");
    if(frame.tick==last_tick_)return std::nullopt;
    const auto world=root_+R::CGameClient::m_PredictedWorld,collision=root_+R::CGameClient::m_Collision,snap=root_+R::CGameClient::m_Snap;
    Replica::Need(Int(world+R::CGameWorld::m_GameTick)==frame.tick,"world/prediction tick disagreement");
    frame.client_id=Int(snap+R::Snap::m_LocalClientId);
    Replica::Need(frame.client_id>=0 && frame.client_id<MAX_CLIENTS && Int(root_+R::CGameClient::m_aLocalIds+slot*4)==frame.client_id &&
        Int(world+R::CGameWorld::m_LocalClientId)==frame.client_id,"local/dummy/world identity disagreement");
    Replica::Need(Int(snap+R::Snap::m_NumPlayers)==1,"preview requires exactly one player; multiplayer replica is unfinished");
    Replica::Need(Read(snap+R::Snap::m_aCharacters+static_cast<std::size_t>(frame.client_id)*R::sizeof_CGameClient_CSnapState_CCharacterInfo+R::SnapCharacter::m_Active,1)[0]==1,"no active local character");
    const auto cores=Read(world,MAX_CLIENTS*8),characters=Read(world+R::CGameWorld::m_apCharacters,MAX_CLIENTS*8);
    const auto local=static_cast<std::size_t>(frame.client_id),core=Replica::U64(cores,local*8);
    Replica::Need(helper::UserRange(core,R::sizeof_CCharacterCore) && core>=R::CCharacter::m_Core,"no predicted local core");
    const auto character=core-R::CCharacter::m_Core;
    for(std::size_t id=0;id<MAX_CLIENTS;++id)
        Replica::Need((id==local && Replica::U64(characters,id*8)==character) || (id!=local && Replica::U64(characters,id*8)==0 && Replica::U64(cores,id*8)==0),"other predicted characters are not yet replicated");
    const auto entities=Read(world+R::CGameWorld::m_apFirstEntityTypes,CGameWorld::NUM_ENTTYPES*8);
    for(std::size_t type=0;type<CGameWorld::NUM_ENTTYPES;++type)
        Replica::Need(Replica::U64(entities,type*8)==(type==CGameWorld::ENTTYPE_CHARACTER ? character : 0),"dynamic entities are not yet replicated");
    Replica::Need(Pointer(world+R::CGameWorld::m_pCollision)==collision,"world/collision backlink disagreement");
    const auto tuning_list=Pointer(world+R::CGameWorld::m_pTuningList);
    Replica::Need(tuning_list==root_+R::CGameClient::m_aTuningList,"world/effective tuning table backlink disagreement");
    const auto effective_tuning=Read(tuning_list,R::sizeof_CTuningParams);
    const auto ego=Pointer(world+R::CGameWorld::m_pEgoTilesPrediction);
    if(ego!=0)
    {
        Replica::Need(R::CEgoTilesPrediction::m_vQuads.has_value(),"ego-quad layout acquisition is pending");
        Replica::Need(ego==root_+R::CGameClient::m_EgoTilesPrediction,"ego component backlink disagreement");
        const auto quads=Read(ego+*R::CEgoTilesPrediction::m_vQuads,24);
        Replica::Need(Replica::U64(quads,0)==Replica::U64(quads,8),"ego prediction quads are not yet replicated");
    }
    const auto collision_bytes=Read(collision,160);
    const int width=Replica::Int(collision_bytes,R::CCollision::m_Width),height=Replica::Int(collision_bytes,R::CCollision::m_Height);
    Replica::Need(width>0 && height>0 && static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)<=MAX_PREVIEW_TILES,"preview map-size bound");
    for(auto field : {R::CCollision::m_pTele,R::CCollision::m_pSpeedup,R::CCollision::m_pSwitch,R::CCollision::m_pTune,R::CCollision::m_pDoor,R::CCollision::m_pExtraDoorTiles})
        Replica::Need(Replica::U64(collision_bytes,field)==0,"tele/speedup/switch/tune/extra-door layers are not yet replicated");
    const auto tile_pointer=Replica::U64(collision_bytes,R::CCollision::m_pTiles),front_pointer=Replica::U64(collision_bytes,R::CCollision::m_pFront);
    const auto tile_bytes=static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*4;
    const auto game=Read(tile_pointer,tile_bytes),front=front_pointer ? Read(front_pointer,tile_bytes) : std::vector<std::uint8_t>{};
    if(!map_ || width!=map_->width || height!=map_->height || game!=map_->game || front!=map_->front)
    {
        for(const auto *layer : {&game,&front})for(std::size_t at=0;at<layer->size();at+=4)
            Replica::Need(SupportedStaticMapTile((*layer)[at]),"unsupported map tile/entity in this preview");
        auto next=std::make_shared<StaticMap>();next->width=width;next->height=height;next->game=game;next->front=front;next->generation=++generation_;map_=std::move(next);
    }
    frame.map=map_;
    frame.character=Read(character,R::sizeof_CCharacter);
    const auto core_bytes=std::span(frame.character).subspan(R::CCharacter::m_Core,R::sizeof_CCharacterCore);
    Replica::Need(Replica::U64(core_bytes,R::CCharacterCore::m_pWorld)==world && Replica::U64(core_bytes,R::CCharacterCore::m_pCollision)==collision &&
        Replica::U64(frame.character,R::CEntity::m_pGameWorld)==world && Replica::Int(frame.character,R::CEntity::m_Id)==frame.client_id &&
        Replica::Int(core_bytes,R::CCharacterCore::m_Id)==frame.client_id,"character/core backlinks or ids disagree");
    Replica::Need(Replica::Bool(frame.character,R::CCharacter::m_IsLocal),"predicted character is not marked local");
    for(std::size_t parameter=0;parameter<R::sizeof_CTuningParams/4;++parameter)
        Replica::Need(Replica::Int(core_bytes,R::CCharacterCore::m_Tuning+parameter*4)==Replica::Int(effective_tuning,parameter*4),"character/effective tuning table disagreement");
    Replica::Need(Replica::Vector(core_bytes,R::CCharacterCore::m_Pos)==Replica::Vector(frame.character,R::CEntity::m_Pos),"entity/core position disagreement");
    const auto client_copy=Read(root_+R::CGameClient::m_aClients+local*R::sizeof_CGameClient_CClientData+R::ClientData::m_Predicted,R::sizeof_CCharacterCore);
    Replica::Need(SameCorePrimitives(core_bytes,client_copy),"predicted-world/client-copy disagreement");
    const auto controls_address=root_+R::CGameClient::m_Controls+R::Controls::m_aInputData+slot*40;
    const auto controls=Read(controls_address,40);frame.controls=Replica::Input(controls,0);
    Replica::Need((frame.controls.m_PlayerFlags&(PLAYERFLAG_IN_MENU|PLAYERFLAG_CHATTING))==0,"menu or chat is active");
    Replica::Need((frame.controls.m_Fire&1)==0 && frame.controls.m_WantedWeapon==0,"weapon-use state is not yet validated");
    Replica::Need(ExactBinding(KEY_A,"+left") && ExactBinding(KEY_D,"+right"),"unmodified A/D single-command movement binds required");
#define READ_WORLD_FLAG(Name) frame.flags.Name=Replica::Bool(Read(world+R::WorldConfig::Name,1),0)
    READ_WORLD_FLAG(m_IsDDRace);READ_WORLD_FLAG(m_IsVanilla);READ_WORLD_FLAG(m_IsFNG);READ_WORLD_FLAG(m_InfiniteAmmo);READ_WORLD_FLAG(m_PredictTiles);
    frame.flags.m_PredictFreeze=Int(world+R::WorldConfig::m_PredictFreeze);
    READ_WORLD_FLAG(m_PredictWeapons);READ_WORLD_FLAG(m_PredictDDRace);READ_WORLD_FLAG(m_IsSolo);READ_WORLD_FLAG(m_UseTuneZones);READ_WORLD_FLAG(m_BugDDRaceInput);
    READ_WORLD_FLAG(m_NoWeakHookAndBounce);READ_WORLD_FLAG(m_PredictEvents);READ_WORLD_FLAG(m_OldLaser);READ_WORLD_FLAG(m_PredictTeleports);
#undef READ_WORLD_FLAG
    Replica::Need(frame.flags.m_IsDDRace && frame.flags.m_PredictDDRace && frame.flags.m_PredictTiles && frame.flags.m_PredictFreeze>=0 && frame.flags.m_PredictFreeze<=2,"unsupported prediction mode");
    std::size_t cfg_index=0;
#define READ_CONFIG(Name) frame.config[cfg_index++]=Int(config+R::CConfig::m_##Name);
    MALINKA_PREDICTION_CONFIG(READ_CONFIG)
#undef READ_CONFIG
    Replica::Need(cfg_index==CONFIG_COUNT,"configuration count");
    for(int value : frame.config)Replica::Need(value>=-1000000 && value<=1000000,"prediction configuration bound");
    // Double collection rejects observed changes; no remote seqlock is claimed.
    Replica::Need(Read(character,R::sizeof_CCharacter)==frame.character && Read(controls_address,40)==controls && Read(tile_pointer,tile_bytes)==game &&
        (!front_pointer || Read(front_pointer,tile_bytes)==front) && Read(collision,160)==collision_bytes &&
        Pointer(world+R::CGameWorld::m_pTuningList)==tuning_list && Read(tuning_list,R::sizeof_CTuningParams)==effective_tuning,"snapshot changed during collection");
    cfg_index=0;
#define RECHECK_CONFIG(Name) Replica::Need(frame.config[cfg_index++]==Int(config+R::CConfig::m_##Name),"prediction configuration changed");
    MALINKA_PREDICTION_CONFIG(RECHECK_CONFIG)
#undef RECHECK_CONFIG
    Replica::Need(Int(engine+R::IClient::m_State)==IClient::STATE_ONLINE && Int(engine+R::IClient::m_aPredTick+slot*4)==frame.tick &&
        Int(world+R::CGameWorld::m_GameTick)==frame.tick && Int(world+R::CGameWorld::m_LocalClientId)==frame.client_id && Int(config+R::CConfig::m_ClDummy)==frame.dummy &&
        Int(snap+R::Snap::m_LocalClientId)==frame.client_id && Pointer(root_+R::CGameClient::m_pClient)==engine &&
        Pointer(root_+R::CGameClient::m_pConfig)==config && Pointer(root_)==module_+helper::PUBLIC_311.vtable_rva && Alive(),"identity or tick changed during collection");
    Replica::Need(helper::MonotonicMs()-frame.measured_ms<=15,"snapshot read-time budget");
    last_tick_=frame.tick;return frame;
}
} // namespace malinka::avoid
