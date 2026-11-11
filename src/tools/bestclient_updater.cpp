/* Copyright © 2026 BestProject Team */
#include <windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdlib.h>

#include <zlib.h>
#include "malinka_package_zip.h"
#include <memory>

#include <atomic>
#include <cwchar>
#include <limits>
#include <exception>
#include <functional>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")

static const int WND_W = 480;
static const int WND_H = 215;

static const COLORREF C_BG         = RGB(22,  22,  22);
static const COLORREF C_GREEN      = RGB(105, 190, 70);
static const COLORREF C_ORANGE     = RGB(230, 80,  45);
static const COLORREF C_TITLE      = RGB(235, 245, 232);
static const COLORREF C_DIM        = RGB(140, 155, 135);
static const COLORREF C_BAR_BG     = RGB(40,  40,  40);
static const COLORREF C_ERROR      = RGB(230, 75,  45);

static std::atomic<HWND> g_hWnd = nullptr;
static std::atomic<int>  g_Percent  = 0;
static std::atomic<bool> g_Failed = false;
static CRITICAL_SECTION  g_Lock;
static wchar_t           g_aStatus[256] = L"Starting...";

#define WM_WORKER_TICK (WM_APP + 0)
#define WM_WORKER_DONE (WM_APP + 1)

static void SetStatus(const wchar_t *pText)
{
	EnterCriticalSection(&g_Lock);
	wcsncpy_s(g_aStatus, pText, _TRUNCATE);
	LeaveCriticalSection(&g_Lock);
	if(g_hWnd)
		PostMessage(g_hWnd, WM_WORKER_TICK, 0, 0);
}

static void SetPercent(int Pct)
{
	if(Pct < 0) Pct = 0;
	if(Pct > 100) Pct = 100;
	g_Percent.store(Pct);
	if(g_hWnd)
		PostMessage(g_hWnd, WM_WORKER_TICK, 0, 0);
}

static std::wstring Widen(const std::string &Text)
{
	if(Text.empty())
		return {};
	const int Count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.data(), static_cast<int>(Text.size()), nullptr, 0);
	if(Count <= 0)
		return L"Update failed (invalid UTF-8 error message).";
	std::wstring Result(Count, L'\0');
	if(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.data(), static_cast<int>(Text.size()), Result.data(), Count) != Count)
		return L"Update failed (message conversion error).";
	return Result;
}

static void Fail(const wchar_t *pMsg)
{
	g_Failed = true;
	SetStatus(pMsg);

}

struct WorkerArgs
{
	DWORD Pid = 0;
	std::wstring aArchive;
	std::wstring aInstallDir;
	std::wstring aExePath;
	std::string ArchiveSha256;
	std::string Version;
};

static DWORD WINAPI WorkerThread(LPVOID pParam) try
{
	std::unique_ptr<WorkerArgs> pA(static_cast<WorkerArgs *>(pParam));
	auto Failure = [](const std::string &Error) { const auto Wide = Widen(Error); Fail(Wide.c_str()); return 1u; };
	if(!pA->Pid || !Malinka::Package::IsHex(pA->ArchiveSha256, 64))
		return Failure("Invalid updater PID or expected archive digest.");
	SetStatus(L"Waiting for client to close...");
	HANDLE Process = OpenProcess(SYNCHRONIZE, FALSE, pA->Pid);
	if(Process)
	{
		const DWORD Wait = WaitForSingleObject(Process, 60000);
		CloseHandle(Process);
		if(Wait != WAIT_OBJECT_0)
			return Failure("Client did not close; installation cancelled.");
	}
	else if(GetLastError() != ERROR_INVALID_PARAMETER)
		return Failure("Cannot confirm the client has exited; installation cancelled.");
	std::string Error, Hash;
	uint64_t Size = 0;
	if(!Malinka::Package::HashFile(pA->aArchive, Hash, Size) || Hash != pA->ArchiveSha256)
		return Failure("Downloaded archive hash changed; installation cancelled.");
	const auto InstallDir = std::filesystem::path(pA->aInstallDir);
	const auto Extract = InstallDir / "update" / ("malinka-extract-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	SetStatus(L"Extracting checked Malinka package...");
	if(!Malinka::Package::ExtractZip(pA->aArchive, Extract, Error))
		return Failure(Error);
	Malinka::Package::SManifest Manifest;
	if(!Malinka::Package::ReadManifest(Extract, Manifest, Error) || Manifest.m_Version != pA->Version)
		return Failure(Error.empty() ? "Package version differs from selected release." : Error);
	SetStatus(L"Installing with rollback backup...");
	if(!Malinka::Package::Install(Extract, InstallDir, std::filesystem::path(pA->aExePath), Malinka::Package::Platform(), Error, [](int Percent) { SetPercent(50 + Percent / 2); }))
		return Failure(Error);
	std::error_code Ec;
	std::filesystem::remove_all(Extract, Ec);
	SetStatus(L"Launching BestClient-Malinka...");
	SHELLEXECUTEINFOW Info{};
	Info.cbSize = sizeof(Info);
	Info.lpVerb = L"open";
	Info.lpFile = pA->aExePath.c_str();
	Info.lpDirectory = pA->aInstallDir.c_str();
	Info.nShow = SW_SHOWNORMAL;
	if(!ShellExecuteExW(&Info))
		return Failure("Update installed, but launching the client failed. Start it manually.");
	if(g_hWnd)
		PostMessage(g_hWnd, WM_WORKER_DONE, 0, 0);
	return 0;
}
catch(const std::exception &)
{
	Fail(L"Update cancelled: allocation or filesystem exception. Check rollback backup before restarting.");
	return 1;
}

static COLORREF LerpColor(COLORREF A, COLORREF B, float T)
{
	int R = (int)(GetRValue(A) + T * (GetRValue(B) - GetRValue(A)));
	int G = (int)(GetGValue(A) + T * (GetGValue(B) - GetGValue(A)));
	int Bl = (int)(GetBValue(A) + T * (GetBValue(B) - GetBValue(A)));
	return RGB(R, G, Bl);
}

static void DrawGradientH(HDC Dc, RECT Rc, COLORREF C1, COLORREF C2)
{
	int W = Rc.right - Rc.left;
	if(W <= 0) return;
	for(int X = 0; X < W; ++X)
	{
		float T = (float)X / (W > 1 ? W - 1 : 1);
		HBRUSH Br = CreateSolidBrush(LerpColor(C1, C2, T));
		RECT Col = {Rc.left + X, Rc.top, Rc.left + X + 1, Rc.bottom};
		FillRect(Dc, &Col, Br);
		DeleteObject(Br);
	}
}

static HFONT MakeFont(int Size, bool Bold, const wchar_t *pFace = L"Segoe UI")
{
	return CreateFontW(Size, 0, 0, 0, Bold ? FW_BOLD : FW_NORMAL,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, pFace);
}

static void Paint(HWND hWnd)
{
	PAINTSTRUCT Ps;
	HDC Dc = BeginPaint(hWnd, &Ps);

	RECT Rc; GetClientRect(hWnd, &Rc);


	HDC Mem = CreateCompatibleDC(Dc);
	HBITMAP Bmp = CreateCompatibleBitmap(Dc, Rc.right, Rc.bottom);
	SelectObject(Mem, Bmp);


	{
		HBRUSH Br = CreateSolidBrush(C_BG);
		FillRect(Mem, &Rc, Br);
		DeleteObject(Br);
	}


	{
		RECT Bar = {0, 0, Rc.right, 4};
		DrawGradientH(Mem, Bar, C_GREEN, C_ORANGE);
	}

	SetBkMode(Mem, TRANSPARENT);


	{
		HFONT F = MakeFont(32, true);
		HFONT Old = (HFONT)SelectObject(Mem, F);
		SetTextColor(Mem, C_TITLE);
		RECT R = {0, 10, Rc.right, 52};
		DrawTextW(Mem, L"BestClient", -1, &R, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		SelectObject(Mem, Old);
		DeleteObject(F);
	}


	{
		HFONT F = MakeFont(32, true);
		HFONT Old = (HFONT)SelectObject(Mem, F);
		SetTextColor(Mem, C_TITLE);
		RECT R = {0, 44, Rc.right, 86};
		DrawTextW(Mem, L"Updater", -1, &R, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		SelectObject(Mem, Old);
		DeleteObject(F);
	}


	const int BarL  = 40;
	const int BarR  = Rc.right - 80;
	const int BarT  = 110;
	const int BarB  = 134;
	const int BarH  = BarB - BarT;


	{
		RECT R = {BarL, BarT, BarR, BarB};
		HBRUSH Br = CreateSolidBrush(C_BAR_BG);
		FillRect(Mem, &R, Br);
		DeleteObject(Br);
	}


	int Pct      = g_Percent.load();
	int FillW    = (BarR - BarL) * Pct / 100;
	if(FillW > 0)
	{


		COLORREF FillEnd = LerpColor(C_GREEN, C_ORANGE, (float)Pct / 100.0f);
		RECT R = {BarL, BarT, BarL + FillW, BarB};
		if(g_Failed)
		{
			HBRUSH Br = CreateSolidBrush(C_ERROR);
			FillRect(Mem, &R, Br);
			DeleteObject(Br);
		}
		else
		{
			DrawGradientH(Mem, R, C_GREEN, FillEnd);
		}
	}


	{
		HFONT F = MakeFont(13, true);
		HFONT Old = (HFONT)SelectObject(Mem, F);
		SetTextColor(Mem, g_Failed ? C_ERROR : C_TITLE);
		wchar_t aBuf[8];
		_snwprintf_s(aBuf, _TRUNCATE, L"%d%%", Pct);
		RECT R = {BarR + 6, BarT, Rc.right - 4, BarB};
		DrawTextW(Mem, aBuf, -1, &R, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
		SelectObject(Mem, Old);
		DeleteObject(F);
	}


	{
		HFONT F = MakeFont(13, false);
		HFONT Old = (HFONT)SelectObject(Mem, F);
		SetTextColor(Mem, g_Failed ? C_ERROR : C_DIM);
		EnterCriticalSection(&g_Lock);
		wchar_t aStatus[256];
		wcscpy_s(aStatus, g_aStatus);
		LeaveCriticalSection(&g_Lock);
		RECT R = {BarL, BarB + 10, Rc.right - BarL, BarB + 36};
		DrawTextW(Mem, aStatus, -1, &R, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
		SelectObject(Mem, Old);
		DeleteObject(F);
	}


	if(g_Failed)
	{
		HFONT F = MakeFont(11, false);
		HFONT Old = (HFONT)SelectObject(Mem, F);
		SetTextColor(Mem, C_DIM);
		RECT R = {BarL, Rc.bottom - 22, Rc.right - BarL, Rc.bottom - 4};
		DrawTextW(Mem, L"Press Esc to close.", -1, &R, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
		SelectObject(Mem, Old);
		DeleteObject(F);
	}

	BitBlt(Dc, 0, 0, Rc.right, Rc.bottom, Mem, 0, 0, SRCCOPY);
	DeleteObject(Bmp);
	DeleteDC(Mem);
	EndPaint(hWnd, &Ps);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg)
	{
	case WM_PAINT:        Paint(hWnd); return 0;
	case WM_ERASEBKGND:  return 1;
	case WM_WORKER_TICK: InvalidateRect(hWnd, NULL, FALSE); return 0;
	case WM_WORKER_DONE: DestroyWindow(hWnd); return 0;
	case WM_DESTROY:
		g_hWnd = nullptr;     PostQuitMessage(0); return 0;

	case WM_NCHITTEST:
		if(DefWindowProcW(hWnd, Msg, wParam, lParam) == HTCLIENT)
			return HTCAPTION;
		return DefWindowProcW(hWnd, Msg, wParam, lParam);
	case WM_KEYDOWN:
		if(wParam == VK_ESCAPE) DestroyWindow(hWnd);
		return 0;
	}
	return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

// Destruction order is deliberate: the worker is joined before the UI status
// critical section is destroyed, including exception/early-return paths.
struct SCriticalSectionLifetime
{
	SCriticalSectionLifetime() { InitializeCriticalSection(&g_Lock); }
	~SCriticalSectionLifetime() { DeleteCriticalSection(&g_Lock); }
};
struct SWorkerLifetime
{
	HANDLE m_Handle = nullptr;
	~SWorkerLifetime()
	{
		if(m_Handle)
		{
			WaitForSingleObject(m_Handle, INFINITE);
			CloseHandle(m_Handle);
		}
	}
};

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) try
{
	OutputDebugStringA(MALINKA_UPDATER_MARKER);
	SCriticalSectionLifetime LockLifetime;


	int Argc = 0;
	const auto FreeArguments = [](LPWSTR *pArguments) { if(pArguments) LocalFree(pArguments); };
	std::unique_ptr<LPWSTR, decltype(FreeArguments)> Arguments(CommandLineToArgvW(GetCommandLineW(), &Argc), FreeArguments);
	if(Argc != 7 || !Arguments)
	{
		MessageBoxW(nullptr,
			L"Usage: bestclient-updater.exe <pid> <archive> <install_dir> <exe> <sha256> <version>",
			L"BestClient-Malinka Updater", MB_ICONERROR);
		return 1;
	}
	LPWSTR *ppArgv = Arguments.get();
	uint64_t ParsedPid = 0;
	const size_t PidLength = std::wcslen(ppArgv[1]);
	bool ValidPid = PidLength > 0 && PidLength <= 10;
	for(size_t Index = 0; ValidPid && Index < PidLength; ++Index)
	{
		const wchar_t Digit = ppArgv[1][Index];
		if(Digit < L'0' || Digit > L'9')
			ValidPid = false;
		else
			ParsedPid = ParsedPid * 10 + static_cast<uint64_t>(Digit - L'0');
	}
	if(!ValidPid || ParsedPid <= 1 || ParsedPid > std::numeric_limits<DWORD>::max() || ParsedPid == GetCurrentProcessId() ||
		std::wcslen(ppArgv[2]) > 32767 || std::wcslen(ppArgv[3]) > 32767 || std::wcslen(ppArgv[4]) > 32767 ||
		std::wcslen(ppArgv[5]) != 64 || std::wcslen(ppArgv[6]) > 32)
	{
		MessageBoxW(nullptr, L"Invalid updater arguments. No files were changed.", L"BestClient-Malinka Updater", MB_ICONERROR);
		return 1;
	}
	auto pArgs = std::make_unique<WorkerArgs>();
	pArgs->Pid = static_cast<DWORD>(ParsedPid);
	pArgs->aArchive = ppArgv[2];
	pArgs->aInstallDir = ppArgv[3];
	pArgs->aExePath = ppArgv[4];
	pArgs->ArchiveSha256 = Malinka::Package::Utf8(std::filesystem::path(ppArgv[5]));
	pArgs->Version = Malinka::Package::Utf8(std::filesystem::path(ppArgv[6]));
	Arguments.reset();

	WNDCLASSEXW Wc      = {};
	Wc.cbSize           = sizeof(Wc);
	Wc.lpfnWndProc      = WndProc;
	Wc.hInstance        = hInst;
	Wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
	Wc.hbrBackground    = NULL;
	Wc.lpszClassName    = L"BCUpdater";
	Wc.hIcon            = LoadIcon(NULL, IDI_APPLICATION);
	if(!RegisterClassExW(&Wc))
		return 1;

	int X = (GetSystemMetrics(SM_CXSCREEN) - WND_W) / 2;
	int Y = (GetSystemMetrics(SM_CYSCREEN) - WND_H) / 2;

	g_hWnd = CreateWindowExW(
		WS_EX_APPWINDOW,
		L"BCUpdater",
		L"BestClient-Malinka Updater",
		WS_POPUP | WS_VISIBLE,
		X, Y, WND_W, WND_H,
		NULL, NULL, hInst, NULL);

	if(!g_hWnd) return 1;


	SWorkerLifetime Worker;
	Worker.m_Handle = CreateThread(nullptr, 0, WorkerThread, pArgs.get(), 0, nullptr);
	if(!Worker.m_Handle)
	{
		DestroyWindow(g_hWnd.load());
		return 1;
	}
	pArgs.release(); // WorkerThread now owns the arguments.

	MSG Msg{};
	BOOL MessageResult;
	while((MessageResult = GetMessageW(&Msg, nullptr, 0, 0)) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessageW(&Msg);
	}
	WaitForSingleObject(Worker.m_Handle, INFINITE);
	DWORD WorkerExit = 1;
	if(!GetExitCodeThread(Worker.m_Handle, &WorkerExit) || MessageResult == -1)
		return 1;
	return static_cast<int>(WorkerExit);
}
catch(const std::exception &)
{
	MessageBoxW(nullptr, L"Cannot initialize the updater. No installation was started.", L"BestClient-Malinka Updater", MB_ICONERROR);
	return 1;
}
