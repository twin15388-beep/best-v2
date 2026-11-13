#include "proxy_version.h"
#include "avoid_hook.h"

#ifdef _WIN32
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    (void)lpReserved;
    switch(ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        if(!avoidfreeze::proxy::InitProxyVersion())
            return FALSE;
        avoidfreeze::proxy::StartHookThread();
        break;

    case DLL_PROCESS_DETACH:
        avoidfreeze::proxy::StopHookThread();
        avoidfreeze::proxy::ShutdownProxyVersion();
        break;
    }
    return TRUE;
}
#endif
