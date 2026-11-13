#include "avoid_hook.h"
#include "avoid_physics.h"

#include <atomic>

#ifdef _WIN32
#include <windows.h>

namespace
{

using FnPrepareInputForSend = void(*)(void *pGameClient, int *pData, int Size, bool Dummy);

FnPrepareInputForSend g_pfnOrigPrepareInputForSend = nullptr;
uintptr_t *g_pHookedVtableSlot = nullptr;

std::atomic<bool> g_Enabled{true};
std::atomic<bool> g_StopThread{false};
HANDLE g_hThread = nullptr;

void Hook_PrepareInputForSend(void *pGameClient, int *pData, int Size, bool Dummy)
{
    if(g_pfnOrigPrepareInputForSend)
    {
        g_pfnOrigPrepareInputForSend(pGameClient, pData, Size, Dummy);
    }

    // Toggle hotkey: F8
    if((GetAsyncKeyState(VK_F8) & 1) != 0)
    {
        const bool newState = !g_Enabled.load(std::memory_order_relaxed);
        g_Enabled.store(newState, std::memory_order_relaxed);
        MessageBeep(newState ? MB_OK : MB_ICONHAND);
    }

    if(!g_Enabled.load(std::memory_order_relaxed) || !pGameClient || !pData || Size < 40)
        return;

    const auto *base_gc = reinterpret_cast<const uint8_t*>(pGameClient);
    const auto *col_ptr = base_gc + avoidfreeze::proxy::GAMECLIENT_COLLISION;

    avoidfreeze::proxy::CollisionView col;
    col.width = *reinterpret_cast<const int*>(col_ptr + avoidfreeze::proxy::COLLISION_WIDTH);
    col.height = *reinterpret_cast<const int*>(col_ptr + avoidfreeze::proxy::COLLISION_HEIGHT);
    col.tiles = *reinterpret_cast<const avoidfreeze::proxy::Tile* const*>(col_ptr + avoidfreeze::proxy::COLLISION_TILES);
    col.front = *reinterpret_cast<const avoidfreeze::proxy::Tile* const*>(col_ptr + avoidfreeze::proxy::COLLISION_FRONT);

    if(!col.IsValid())
        return;

    const int local_id = *reinterpret_cast<const int*>(base_gc + avoidfreeze::proxy::GAMECLIENT_LOCAL_IDS + (Dummy ? sizeof(int) : 0));
    if(local_id < 0 || local_id >= 64)
        return;

    const uint8_t *core = base_gc + avoidfreeze::proxy::GAMECLIENT_CLIENT_DATA +
                          (static_cast<size_t>(local_id) * avoidfreeze::proxy::CLIENT_DATA_SIZE) +
                          avoidfreeze::proxy::CLIENT_DATA_PREDICTED_CORE;

    const bool in_freeze = *reinterpret_cast<const bool*>(core + avoidfreeze::proxy::CORE_IN_FREEZE);
    const bool deep_frozen = *reinterpret_cast<const bool*>(core + avoidfreeze::proxy::CORE_DEEP_FROZEN);
    const bool live_frozen = *reinterpret_cast<const bool*>(core + avoidfreeze::proxy::CORE_LIVE_FROZEN);
    const bool super = *reinterpret_cast<const bool*>(core + avoidfreeze::proxy::CORE_SUPER);
    const bool invincible = *reinterpret_cast<const bool*>(core + avoidfreeze::proxy::CORE_INVINCIBLE);

    if(in_freeze || deep_frozen || live_frozen || super || invincible)
        return;

    const avoidfreeze::proxy::Vec2 pos = *reinterpret_cast<const avoidfreeze::proxy::Vec2*>(core + avoidfreeze::proxy::CORE_POSITION);
    const avoidfreeze::proxy::Vec2 vel = *reinterpret_cast<const avoidfreeze::proxy::Vec2*>(core + avoidfreeze::proxy::CORE_VELOCITY);

    const int current_dir = pData[0];
    const int safe_dir = avoidfreeze::proxy::ResolveSafeDirection(col, pos, vel, current_dir);

    if(safe_dir != current_dir)
    {
        pData[0] = safe_dir;
    }
}

DWORD WINAPI HookWorkerThread(LPVOID)
{
    while(!g_StopThread.load(std::memory_order_relaxed))
    {
        const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if(base != 0)
        {
            auto *vtable = reinterpret_cast<uintptr_t*>(base + avoidfreeze::proxy::GAMECLIENT_VTABLE_RVA);
            if(vtable[avoidfreeze::proxy::VTABLE_PREPARE_INPUT_FOR_SEND] != 0 &&
               vtable[avoidfreeze::proxy::VTABLE_PREPARE_INPUT_FOR_SEND] != reinterpret_cast<uintptr_t>(&Hook_PrepareInputForSend))
            {
                DWORD old_prot = 0;
                if(VirtualProtect(&vtable[avoidfreeze::proxy::VTABLE_PREPARE_INPUT_FOR_SEND], sizeof(uintptr_t), PAGE_READWRITE, &old_prot))
                {
                    g_pfnOrigPrepareInputForSend = reinterpret_cast<FnPrepareInputForSend>(vtable[avoidfreeze::proxy::VTABLE_PREPARE_INPUT_FOR_SEND]);
                    vtable[avoidfreeze::proxy::VTABLE_PREPARE_INPUT_FOR_SEND] = reinterpret_cast<uintptr_t>(&Hook_PrepareInputForSend);
                    VirtualProtect(&vtable[avoidfreeze::proxy::VTABLE_PREPARE_INPUT_FOR_SEND], sizeof(uintptr_t), old_prot, &old_prot);
                    g_pHookedVtableSlot = &vtable[avoidfreeze::proxy::VTABLE_PREPARE_INPUT_FOR_SEND];
                    break;
                }
            }
        }
        Sleep(100);
    }
    return 0;
}

} // namespace

namespace avoidfreeze::proxy
{

bool StartHookThread()
{
    g_StopThread.store(false);
    g_hThread = CreateThread(nullptr, 0, HookWorkerThread, nullptr, 0, nullptr);
    return g_hThread != nullptr;
}

void StopHookThread()
{
    g_StopThread.store(true);
    if(g_pHookedVtableSlot && g_pfnOrigPrepareInputForSend)
    {
        DWORD old_prot = 0;
        if(VirtualProtect(g_pHookedVtableSlot, sizeof(uintptr_t), PAGE_READWRITE, &old_prot))
        {
            *g_pHookedVtableSlot = reinterpret_cast<uintptr_t>(g_pfnOrigPrepareInputForSend);
            VirtualProtect(g_pHookedVtableSlot, sizeof(uintptr_t), old_prot, &old_prot);
        }
        g_pHookedVtableSlot = nullptr;
    }
    if(g_hThread)
    {
        WaitForSingleObject(g_hThread, 500);
        CloseHandle(g_hThread);
        g_hThread = nullptr;
    }
}

bool IsAvoidFreezeEnabled()
{
    return g_Enabled.load(std::memory_order_relaxed);
}

void SetAvoidFreezeEnabled(bool enabled)
{
    g_Enabled.store(enabled, std::memory_order_relaxed);
}

} // namespace avoidfreeze::proxy

#else

namespace avoidfreeze::proxy
{
static std::atomic<bool> g_Enabled{true};

bool StartHookThread() { return true; }
void StopHookThread() {}
bool IsAvoidFreezeEnabled() { return g_Enabled.load(); }
void SetAvoidFreezeEnabled(bool enabled) { g_Enabled.store(enabled); }
}

#endif
