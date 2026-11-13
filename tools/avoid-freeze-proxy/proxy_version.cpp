#include "proxy_version.h"

#ifdef _WIN32
#include <string>

namespace
{
HMODULE g_hRealVersion = nullptr;

FARPROC LoadProc(const char *name)
{
    if(!g_hRealVersion) return nullptr;
    return GetProcAddress(g_hRealVersion, name);
}

#define DECLARE_PROC(name) static auto orig_##name = reinterpret_cast<decltype(&Proxy_##name)>(LoadProc(#name))

} // namespace

namespace avoidfreeze::proxy
{

bool InitProxyVersion()
{
    if(g_hRealVersion) return true;

    wchar_t sysDir[MAX_PATH];
    if(GetSystemDirectoryW(sysDir, MAX_PATH) == 0)
        return false;

    std::wstring realDll = std::wstring(sysDir) + L"\\version.dll";
    g_hRealVersion = LoadLibraryW(realDll.c_str());
    return g_hRealVersion != nullptr;
}

void ShutdownProxyVersion()
{
    if(g_hRealVersion)
    {
        FreeLibrary(g_hRealVersion);
        g_hRealVersion = nullptr;
    }
}

} // namespace avoidfreeze::proxy

extern "C"
{

BOOL __stdcall Proxy_GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    DECLARE_PROC(GetFileVersionInfoA);
    return orig_GetFileVersionInfoA ? orig_GetFileVersionInfoA(lptstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

BOOL __stdcall Proxy_GetFileVersionInfoByHandle(int hMem, LPCWSTR lpFileName, int v2, int v3)
{
    DECLARE_PROC(GetFileVersionInfoByHandle);
    return orig_GetFileVersionInfoByHandle ? orig_GetFileVersionInfoByHandle(hMem, lpFileName, v2, v3) : FALSE;
}

BOOL __stdcall Proxy_GetFileVersionInfoExA(DWORD dwFlags, LPCSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    DECLARE_PROC(GetFileVersionInfoExA);
    return orig_GetFileVersionInfoExA ? orig_GetFileVersionInfoExA(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

BOOL __stdcall Proxy_GetFileVersionInfoExW(DWORD dwFlags, LPCWSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    DECLARE_PROC(GetFileVersionInfoExW);
    return orig_GetFileVersionInfoExW ? orig_GetFileVersionInfoExW(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

DWORD __stdcall Proxy_GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle)
{
    DECLARE_PROC(GetFileVersionInfoSizeA);
    return orig_GetFileVersionInfoSizeA ? orig_GetFileVersionInfoSizeA(lptstrFilename, lpdwHandle) : 0;
}

DWORD __stdcall Proxy_GetFileVersionInfoSizeExA(DWORD dwFlags, LPCSTR lpwstrFilename, LPDWORD lpdwHandle)
{
    DECLARE_PROC(GetFileVersionInfoSizeExA);
    return orig_GetFileVersionInfoSizeExA ? orig_GetFileVersionInfoSizeExA(dwFlags, lpwstrFilename, lpdwHandle) : 0;
}

DWORD __stdcall Proxy_GetFileVersionInfoSizeExW(DWORD dwFlags, LPCWSTR lpwstrFilename, LPDWORD lpdwHandle)
{
    DECLARE_PROC(GetFileVersionInfoSizeExW);
    return orig_GetFileVersionInfoSizeExW ? orig_GetFileVersionInfoSizeExW(dwFlags, lpwstrFilename, lpdwHandle) : 0;
}

DWORD __stdcall Proxy_GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle)
{
    DECLARE_PROC(GetFileVersionInfoSizeW);
    return orig_GetFileVersionInfoSizeW ? orig_GetFileVersionInfoSizeW(lptstrFilename, lpdwHandle) : 0;
}

BOOL __stdcall Proxy_GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    DECLARE_PROC(GetFileVersionInfoW);
    return orig_GetFileVersionInfoW ? orig_GetFileVersionInfoW(lptstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

DWORD __stdcall Proxy_VerFindFileA(DWORD uFlags, LPCSTR szFileName, LPCSTR szWinDir, LPCSTR szAppDir, LPSTR szCurDir, PUINT lpuCurDirLen, LPSTR szDestDir, PUINT lpuDestDirLen)
{
    DECLARE_PROC(VerFindFileA);
    return orig_VerFindFileA ? orig_VerFindFileA(uFlags, szFileName, szWinDir, szAppDir, szCurDir, lpuCurDirLen, szDestDir, lpuDestDirLen) : 0;
}

DWORD __stdcall Proxy_VerFindFileW(DWORD uFlags, LPCWSTR szFileName, LPCWSTR szWinDir, LPCWSTR szAppDir, LPWSTR szCurDir, PUINT lpuCurDirLen, LPWSTR szDestDir, PUINT lpuDestDirLen)
{
    DECLARE_PROC(VerFindFileW);
    return orig_VerFindFileW ? orig_VerFindFileW(uFlags, szFileName, szWinDir, szAppDir, szCurDir, lpuCurDirLen, szDestDir, lpuDestDirLen) : 0;
}

DWORD __stdcall Proxy_VerInstallFileA(DWORD uFlags, LPCSTR szSrcFileName, LPCSTR szDestFileName, LPCSTR szSrcDir, LPCSTR szDestDir, LPCSTR szCurDir, LPSTR szTmpFile, PUINT lpuTmpFileLen)
{
    DECLARE_PROC(VerInstallFileA);
    return orig_VerInstallFileA ? orig_VerInstallFileA(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, lpuTmpFileLen) : 0;
}

DWORD __stdcall Proxy_VerInstallFileW(DWORD uFlags, LPCWSTR szSrcFileName, LPCWSTR szDestFileName, LPCWSTR szSrcDir, LPCWSTR szDestDir, LPCWSTR szCurDir, LPWSTR szTmpFile, PUINT lpuTmpFileLen)
{
    DECLARE_PROC(VerInstallFileW);
    return orig_VerInstallFileW ? orig_VerInstallFileW(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, lpuTmpFileLen) : 0;
}

DWORD __stdcall Proxy_VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD cchLang)
{
    DECLARE_PROC(VerLanguageNameA);
    return orig_VerLanguageNameA ? orig_VerLanguageNameA(wLang, szLang, cchLang) : 0;
}

DWORD __stdcall Proxy_VerLanguageNameW(DWORD wLang, LPWSTR szLang, DWORD cchLang)
{
    DECLARE_PROC(VerLanguageNameW);
    return orig_VerLanguageNameW ? orig_VerLanguageNameW(wLang, szLang, cchLang) : 0;
}

BOOL __stdcall Proxy_VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen)
{
    DECLARE_PROC(VerQueryValueA);
    return orig_VerQueryValueA ? orig_VerQueryValueA(pBlock, lpSubBlock, lplpBuffer, puLen) : FALSE;
}

BOOL __stdcall Proxy_VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen)
{
    DECLARE_PROC(VerQueryValueW);
    return orig_VerQueryValueW ? orig_VerQueryValueW(pBlock, lpSubBlock, lplpBuffer, puLen) : FALSE;
}

}

#else

namespace avoidfreeze::proxy
{
bool InitProxyVersion() { return true; }
void ShutdownProxyVersion() {}
}

#endif
