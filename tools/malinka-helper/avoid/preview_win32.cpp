// SPDX-License-Identifier: MIT
// Standalone external preview. Never installed into or injected into the game.
#include "live_win32.h"
#include "input_plan.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>

namespace
{
using namespace malinka;
// Windows Set-1 scan codes for the physical A/D positions; not game offsets.
constexpr WORD SCAN_A=0x1e,SCAN_D=0x20;
constexpr ULONG_PTR OWN_INPUT_TAG=0x4d416c41764f6964ULL;
struct State
{
    std::atomic_bool stop{false},left{false},right{false},arrow_left{false},arrow_right{false},cancel{false},release_failed{false},blocked{false};
    std::atomic_uint toggles{0};
    std::mutex output;
    void Print(std::string_view text)
    {
        std::lock_guard lock(output);std::cout<<text<<std::endl;
    }
};
std::atomic<State *> observer_state{nullptr};
LRESULT CALLBACK ObserveKeyboard(int code,WPARAM message,LPARAM parameter)
{
    auto *const observer=observer_state.load();
    if(code==HC_ACTION && observer)
    {
        const auto &event=*reinterpret_cast<const KBDLLHOOKSTRUCT *>(parameter);
        const bool down=message==WM_KEYDOWN || message==WM_SYSKEYDOWN;
        const bool up=message==WM_KEYUP || message==WM_SYSKEYUP;
        if((down || up) && (event.flags&LLKHF_INJECTED)==0)
        {
            const bool extended=(event.flags&LLKHF_EXTENDED)!=0;
            if(!extended && event.scanCode==SCAN_A)observer->left.store(down);
            if(!extended && event.scanCode==SCAN_D)observer->right.store(down);
            if(extended && event.vkCode==VK_LEFT)observer->arrow_left.store(down);
            if(extended && event.vkCode==VK_RIGHT)observer->arrow_right.store(down);
            if(down && (event.vkCode==VK_ESCAPE || event.vkCode==VK_F1 || event.vkCode==VK_F2 || event.vkCode==VK_RETURN ||
                event.vkCode=='T' || event.vkCode=='Y' || event.vkCode==VK_OEM_2 || event.vkCode==VK_LWIN || event.vkCode==VK_RWIN ||
                event.vkCode==VK_LMENU || event.vkCode==VK_RMENU || event.vkCode==VK_LCONTROL || event.vkCode==VK_RCONTROL))observer->cancel.store(true);
        }
        else if(down && (event.flags&LLKHF_INJECTED)!=0 && event.dwExtraInfo!=OWN_INPUT_TAG &&
            (event.scanCode==SCAN_A || event.scanCode==SCAN_D))observer->cancel.store(true);
    }
    // Passive OS observation only. No event is hidden, blocked or rewritten.
    return CallNextHookEx(nullptr,code,message,parameter);
}
BOOL WINAPI ConsoleControl(DWORD)
{
    if(auto *const observer=observer_state.load()){observer->stop.store(true);return TRUE;}return FALSE;
}
bool ForegroundIs(DWORD pid)
{
    const HWND window=GetForegroundWindow();DWORD measured{};
    return window && GetWindowThreadProcessId(window,&measured)!=0 && measured==pid;
}
bool ModifiersClear()
{
    for(int key : {VK_SHIFT,VK_CONTROL,VK_MENU,VK_LWIN,VK_RWIN})if((GetAsyncKeyState(key)&0x8000)!=0)return false;
    return true;
}
using Physical=avoid::PhysicalKeys;
class OwnKeys
{
    State &state_;
    DWORD pid_;
    bool left_{false},right_{false};
    mutable std::mutex mutex_;
    bool Event(WORD scan,bool down)
    {
        INPUT input{};input.type=INPUT_KEYBOARD;input.ki.wScan=scan;input.ki.dwFlags=KEYEVENTF_SCANCODE|(down ? 0 : KEYEVENTF_KEYUP);input.ki.dwExtraInfo=OWN_INPUT_TAG;
        const bool ok=SendInput(1,&input,sizeof(input))==1;
        if(ok)++events;
        else if(!down)state_.release_failed.store(true);
        return ok;
    }
public:
    std::atomic_uint64_t events{0},corrections{0};
    explicit OwnKeys(State &state,DWORD pid):state_(state),pid_(pid){}
    ~OwnKeys(){Release();}
    bool Any()const {std::lock_guard lock(mutex_);return left_ || right_;}
    int Expected(const Physical &physical)const {std::lock_guard lock(mutex_);return static_cast<int>(physical.right || right_)-static_cast<int>(physical.left || left_);}
    void Release() noexcept
    {
        std::lock_guard lock(mutex_);
        // A physical press of our key takes over ownership. Its ordinary
        // physical key-up releases it; never cancel a known human-held key.
        if(left_)
        {
            if(state_.left.load() || Event(SCAN_A,false))left_=false;
        }
        if(right_)
        {
            if(state_.right.load() || Event(SCAN_D,false))right_=false;
        }
    }
    void Apply(int direction,const Physical &physical)
    {
        std::lock_guard lock(mutex_);
        Replica::Need(ForegroundIs(pid_),"input target lost focus");
        const auto plan=avoid::PlanInput(direction,physical);
        Replica::Need(plan.has_value(),"cannot reverse a physically held movement key");
        const bool left=plan->left,right=plan->right;
        if(left_ && !left)
        {
            if(!state_.left.load())Replica::Need(Event(SCAN_A,false),"SendInput key release failed");
            left_=false;
        }
        if(right_ && !right)
        {
            if(!state_.right.load())Replica::Need(Event(SCAN_D,false),"SendInput key release failed");
            right_=false;
        }
        if(!left_ && left)
        {
            Replica::Need(ForegroundIs(pid_),"focus changed before key-down");
            Replica::Need(Event(SCAN_A,true),"SendInput key-down failed or was blocked");left_=true;++corrections;
        }
        if(!right_ && right)
        {
            Replica::Need(ForegroundIs(pid_),"focus changed before key-down");
            Replica::Need(Event(SCAN_D,true),"SendInput key-down failed or was blocked");right_=true;++corrections;
        }
    }
};
struct Options {DWORD pid{};std::uint64_t run_ms{};bool enable_input{false},arm_once_ready{false};};
std::optional<std::uint64_t> Decimal(std::wstring_view text,std::uint64_t maximum)
{
    if(text.empty() || text.size()>10)return {};
    std::uint64_t value=0;
    for(wchar_t character : text)
    {
        if(character<L'0' || character>L'9')return {};
        const auto digit=static_cast<std::uint64_t>(character-L'0');
        if(value>(maximum-digit)/10)return {};
        value=value*10+digit;
    }
    return value;
}
Options Parse(int count,wchar_t **arguments)
{
    Options options;
    for(int i=1;i<count;++i)
    {
        const std::wstring_view value=arguments[i];
        if(value==L"--enable-input")options.enable_input=true;
        else if(value==L"--arm-once-ready")options.arm_once_ready=true;
        else if(value==L"--pid" || value==L"--run-ms")
        {
            Replica::Need(i+1<count,"missing numeric argument");const auto number=Decimal(arguments[++i],value==L"--pid" ? MAXDWORD : 300000);
            Replica::Need(number.has_value() && *number>0,"invalid numeric argument");
            if(value==L"--pid")options.pid=static_cast<DWORD>(*number);else options.run_ms=*number;
        }
        else throw Replica::Rejected("usage: MalinkaAvoidPreview --pid N [--enable-input] [--arm-once-ready] [--run-ms 1..300000]");
    }
    Replica::Need(options.pid!=0 && (!options.arm_once_ready || options.enable_input),"a PID and explicit input permission are required");return options;
}
bool Transient(std::string_view reason)
{
    return reason=="world/prediction tick disagreement" || reason=="snapshot changed during collection" ||
        reason=="predicted-world/client-copy disagreement" || reason=="identity or tick changed during collection" ||
        reason=="entity/core position disagreement" || reason=="prediction configuration changed";
}
void Run(State &state,const Options &options,const helper::Report &inspection)
{
    std::uint64_t valid_frames=0,matches=0,mismatches=0,unavailable=0,decisions=0;
    try
    {
        avoid::LiveReader reader(inspection);OwnKeys keys(state,reader.Pid());
        helper::Handle single_input;
        if(options.enable_input)
        {
            const auto name=L"Local\\MalinkaAvoidInput-"+std::to_wstring(reader.Pid())+L"-"+std::to_wstring(reader.Creation());
            single_input.Reset(CreateMutexW(nullptr,FALSE,name.c_str()));
            Replica::Need(bool(single_input) && GetLastError()!=ERROR_ALREADY_EXISTS,"another input companion owns this process");
        }
        const auto started=helper::MonotonicMs();std::uint64_t last_valid=started,last_log=0,last_change=0;
        std::atomic_uint64_t watchdog_heartbeat{started};
        std::jthread watchdog([&](std::stop_token token)
        {
            while(!token.stop_requested())
            {
                if(state.stop.load() || helper::MonotonicMs()-watchdog_heartbeat.load()>100 || !ForegroundIs(reader.Pid()))keys.Release();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        unsigned certified=0;bool armed=false,auto_arm_pending=options.arm_once_ready;std::uint64_t observed_input_events=0;
        bool arrow_left=false,arrow_right=false;std::optional<avoid::Frame> previous;
        std::string status="waiting for a supported live state";
        while(!state.stop.load() && reader.Alive() && (!options.run_ms || helper::MonotonicMs()-started<options.run_ms))
        {
            const auto now=helper::MonotonicMs();
            if(state.cancel.exchange(false)) {armed=false;auto_arm_pending=false;certified=0;previous.reset();keys.Release();status="disarmed by a UI/modifier key";}
            if(!ForegroundIs(reader.Pid()) || !ModifiersClear()){armed=false;keys.Release();status="disarmed: target focus or modifier state is unavailable";}
            const auto current_events=keys.events.load();
            if(current_events!=observed_input_events){last_change=now;observed_input_events=current_events;}
            if(state.toggles.exchange(0)&1U)
            {
                if(armed){armed=false;keys.Release();status="disarmed by F8";}
                else if(options.enable_input && certified>=3 && now-last_valid<=100 && ForegroundIs(reader.Pid()) && ModifiersClear())armed=true;
                else status="cannot arm: explicit permission, fresh validated state and foreground focus required";
            }
            try
            {
                auto captured=reader.Capture();
                if(captured)
                {
                    auto frame=std::move(*captured);++valid_frames;last_valid=helper::MonotonicMs();watchdog_heartbeat.store(last_valid);
                    const bool changed=previous && (previous->map->generation!=frame.map->generation || previous->client_id!=frame.client_id || previous->dummy!=frame.dummy || previous->config!=frame.config || previous->flags!=frame.flags);
                    if(changed){certified=0;armed=false;keys.Release();}
                    if(previous && !changed)
                    {
                        switch(avoid::CheckTransition(*previous,frame))
                        {
                        case avoid::Transition::matched:certified=std::min(3U,certified+1);++matches;break;
                        case avoid::Transition::mismatch:certified=0;armed=false;keys.Release();++mismatches;status="disarmed: independent one-step prediction disagreed";break;
                        case avoid::Transition::learning:
                            certified=0;armed=false;keys.Release();status="disarmed: prediction inputs or tick gap changed; recertifying";break;
                        }
                    }
                    arrow_left=reader.ArrowLeftBound();arrow_right=reader.ArrowRightBound();
                    const Physical physical{state.left.load() || (arrow_left && state.arrow_left.load()),state.right.load() || (arrow_right && state.arrow_right.load())};
                    if(auto_arm_pending && certified>=3 && ForegroundIs(reader.Pid()) && ModifiersClear() && !physical.left && !physical.right)
                    {armed=true;auto_arm_pending=false;state.Print("ARMED_ONCE: explicit --arm-once-ready request consumed");}
                    if(certified<3){keys.Release();status="learning: three matched observed transitions required";}
                    else if(!armed)
                    {
                        keys.Release();
                        const auto preview=avoid::Choose(frame,frame.controls.m_Direction,{true,true,true});++decisions;
                        status=(options.enable_input ? "READY: F8 to arm; " : "READY: observe-only; ")+preview.reason;
                    }
                    else
                    {
                        const auto expected=keys.Expected(physical);
                        if(frame.controls.m_Direction!=expected)
                        {
                            Replica::Need(last_change!=0 && helper::MonotonicMs()-last_change<=100,"movement source or input acknowledgement disagreed");
                            status="waiting for ordinary input acknowledgement";
                        }
                        else
                        {
                            const std::array<bool,3> allowed{!physical.right,true,!physical.left};
                            const auto decision=avoid::Choose(frame,physical.Direction(),allowed);++decisions;
                            const auto after=helper::MonotonicMs();
                            Replica::Need(after-frame.measured_ms<=25 && after-last_valid<=25 && ForegroundIs(reader.Pid()) && ModifiersClear() && reader.Alive(),"decision became stale or focus changed");
                            const Physical rechecked{state.left.load() || (arrow_left && state.arrow_left.load()),state.right.load() || (arrow_right && state.arrow_right.load())};
                            Replica::Need(rechecked==physical,"physical movement changed during prediction");
                            const auto before_events=keys.events.load();
                            if(decision.state==avoid::DecisionState::correcting)keys.Apply(decision.direction,physical);
                            else keys.Release();
                            if(keys.events.load()!=before_events)last_change=helper::MonotonicMs();
                            status=decision.reason;
                        }
                    }
                    previous=std::move(frame);
                }
                if(helper::MonotonicMs()-last_valid>100){armed=false;certified=0;previous.reset();keys.Release();status="disarmed: no fresh coherent prediction tick";}
            }
            catch(const std::exception &error)
            {
                ++unavailable;keys.Release();status=error.what();
                if(!Transient(status)){armed=false;certified=0;previous.reset();}
            }
            if(now-last_log>=200)
            {
                last_log=now;
                std::string line="STATE "+status+" armed="+(armed ? "1" : "0")+" matches="+std::to_string(matches)+" mismatches="+std::to_string(mismatches)+" input_events="+std::to_string(keys.events.load());
                if(previous)
                {
                    const auto bytes=std::span(previous->character).subspan(RemoteLayout::CCharacter::m_Core,RemoteLayout::sizeof_CCharacterCore);
                    const auto position=Replica::Vector(bytes,RemoteLayout::CCharacterCore::m_Pos);
                    line+=" tick="+std::to_string(previous->tick)+" x="+std::to_string(position.x)+" y="+std::to_string(position.y)+" freeze_time="+std::to_string(Replica::Int(previous->character,RemoteLayout::CCharacter::m_FreezeTime));
                }
                state.Print(line);
            }
            if(state.release_failed.load()){armed=false;state.Print("STOP: an ordinary key release was blocked; input host is unavailable");break;}
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        watchdog.request_stop();watchdog.join();keys.Release();
        state.Print("RESULT {\"valid_frames\":"+std::to_string(valid_frames)+",\"transition_matches\":"+std::to_string(matches)+",\"transition_mismatches\":"+std::to_string(mismatches)+",\"unavailable_samples\":"+std::to_string(unavailable)+",\"decisions\":"+std::to_string(decisions)+",\"input_events\":"+std::to_string(keys.events.load())+",\"corrections\":"+std::to_string(keys.corrections.load())+",\"owned_keys_held_at_stop\":"+(keys.Any() ? "true" : "false")+",\"release_failed\":"+(state.release_failed.load() ? "true" : "false")+",\"writes_process_memory\":false,\"injects_dll\":false}");
    }
    catch(const std::exception &error){state.blocked.store(true);state.Print("BLOCKED: "+std::string(error.what()));}
    state.stop.store(true);
}
} // namespace
int wmain(int count,wchar_t **arguments) try
{
    if(count==2 && std::wstring_view(arguments[1])==L"--self-test-abi")
    {
        avoid::ValidateNativeReplicaAbi();
        std::cout<<"native-abi-self-test=passed; pinned MSVC x64 candidate only; target binary not validated; process access=none; game input=none"<<std::endl;
        return 0;
    }
    const auto options=Parse(count,arguments);State state;observer_state.store(&state);
    struct ClearObserver {~ClearObserver(){observer_state.store(nullptr);}} clear;
    state.Print("MalinkaAvoidPreview 0.2.0: independent read-only state + ordinary input; NOT full KRX parity.");
    state.Print("F8 toggles explicit input permission; Escape/chat/console/modifier/focus loss disarms. Ctrl+C stops. Installed client files are never patched.");
    helper::Report inspection;
    for(int attempt=0;attempt<8;++attempt)
    {
        inspection=helper::InspectProcess(options.pid);
        if(inspection.process && inspection.process->scan.status=="complete" && inspection.process->scan.candidates.size()==1 && inspection.process->error.empty())break;
        if(!inspection.file.recognized_public_release || (inspection.process && !inspection.process->read_only_handle))break;
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    avoid::ValidateNativeReplicaAbi();
    // A passive low-level OS observer runs in THIS process, never in the game.
    // Observe-only mode does not install it and never invokes SendInput.
    HHOOK hook=nullptr;bool hotkey=false;
    if(options.enable_input)
    {
        state.left.store((GetAsyncKeyState('A')&0x8000)!=0);state.right.store((GetAsyncKeyState('D')&0x8000)!=0);
        state.arrow_left.store((GetAsyncKeyState(VK_LEFT)&0x8000)!=0);state.arrow_right.store((GetAsyncKeyState(VK_RIGHT)&0x8000)!=0);
        hook=SetWindowsHookExW(WH_KEYBOARD_LL,ObserveKeyboard,GetModuleHandleW(nullptr),0);
        Replica::Need(hook!=nullptr,"passive keyboard observer could not be installed");
        hotkey=RegisterHotKey(nullptr,88,MOD_NOREPEAT,VK_F8)!=FALSE;
        if(!hotkey){UnhookWindowsHookEx(hook);throw Replica::Rejected("F8 is already registered; input was not enabled");}
    }
    struct Cleanup
    {
        HHOOK hook;bool hotkey;
        ~Cleanup(){if(hotkey)UnregisterHotKey(nullptr,88);if(hook)UnhookWindowsHookEx(hook);SetConsoleCtrlHandler(ConsoleControl,FALSE);}
    } cleanup{hook,hotkey};
    Replica::Need(SetConsoleCtrlHandler(ConsoleControl,TRUE)!=FALSE,"console shutdown handler could not be installed");
    std::jthread worker([&]{Run(state,options,inspection);});
    while(!state.stop.load())
    {
        MSG message{};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE))
        {
            if(message.message==WM_HOTKEY && message.wParam==88)state.toggles.fetch_add(1);
            if(message.message==WM_QUIT)state.stop.store(true);
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    worker.join();return state.release_failed.load() ? 2 : (state.blocked.load() ? 1 : 0);
}
catch(const std::exception &error){std::cerr<<"BLOCKED: "<<error.what()<<std::endl;return 1;}
