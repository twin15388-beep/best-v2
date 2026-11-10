/* Copyright © 2026 BestProject Team */
#include "bestclient_updater_linux_font.h"
#include "malinka_package_zip.h"

#include <SDL.h>

#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>
#include <memory>

static const int WND_W = 480;
static const int WND_H = 215;

static const SDL_Color C_BG = {22, 22, 22, 255};
static const SDL_Color C_GREEN = {105, 190, 70, 255};
static const SDL_Color C_ORANGE = {230, 80, 45, 255};
static const SDL_Color C_TITLE = {235, 245, 232, 255};
static const SDL_Color C_DIM = {140, 155, 135, 255};
static const SDL_Color C_BAR_BG = {40, 40, 40, 255};
static const SDL_Color C_ERROR = {230, 75, 45, 255};

static SDL_mutex *g_pLock = nullptr;
static SDL_atomic_t g_Percent;
static SDL_atomic_t g_Failed;
static SDL_atomic_t g_Done;
static std::string g_Status = "Starting...";

static void SetStatus(const char *pText)
{
	SDL_LockMutex(g_pLock);
	g_Status = pText;
	SDL_UnlockMutex(g_pLock);
}

static std::string GetStatus()
{
	SDL_LockMutex(g_pLock);
	const std::string Copy = g_Status;
	SDL_UnlockMutex(g_pLock);
	return Copy;
}

static void SetPercent(int Pct)
{
	if(Pct < 0)
		Pct = 0;
	if(Pct > 100)
		Pct = 100;
	SDL_AtomicSet(&g_Percent, Pct);
}

static void Fail(const char *pMsg)
{
	SDL_AtomicSet(&g_Failed, 1);
	SetStatus(pMsg);

}

static bool ProcessGone(pid_t Pid)
{
	return kill(Pid, 0) == -1 && errno == ESRCH;
}

struct SWorkerArgs
{
	pid_t Pid;
	std::string Archive;
	std::string InstallDir;
	std::string ExePath;
	std::string ArchiveSha256;
	std::string Version;
};

static int WorkerThreadFn(void *pData) try
{
	std::unique_ptr<SWorkerArgs> pArgs(static_cast<SWorkerArgs *>(pData));
	auto Failure = [](const std::string &Error) { Fail(Error.c_str()); return 1; };
	if(pArgs->Pid <= 0 || !Malinka::Package::IsHex(pArgs->ArchiveSha256, 64))
		return Failure("Invalid updater PID or expected archive digest.");
	SetStatus("Waiting for client to close...");
	const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
	while(!ProcessGone(pArgs->Pid))
	{
		if(std::chrono::steady_clock::now() >= Deadline)
			return Failure("Client did not close; installation cancelled.");
		usleep(200 * 1000);
	}
	std::string Error, Hash;
	uint64_t Size = 0;
	if(!Malinka::Package::HashFile(pArgs->Archive, Hash, Size) || Hash != pArgs->ArchiveSha256)
		return Failure("Downloaded archive hash changed; installation cancelled.");
	const auto InstallDir = std::filesystem::u8path(pArgs->InstallDir);
	const auto Extract = InstallDir / "update" / ("malinka-extract-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	SetStatus("Extracting checked Malinka package...");
	if(!Malinka::Package::ExtractZip(std::filesystem::u8path(pArgs->Archive), Extract, Error))
		return Failure(Error);
	Malinka::Package::SManifest Manifest;
	if(!Malinka::Package::ReadManifest(Extract, Manifest, Error) || Manifest.m_Version != pArgs->Version)
		return Failure(Error.empty() ? "Package version differs from selected release." : Error);
	SetStatus("Installing with rollback backup...");
	if(!Malinka::Package::Install(Extract, InstallDir, std::filesystem::u8path(pArgs->ExePath), Malinka::Package::Platform(), Error, [](int Percent) { SetPercent(50 + Percent / 2); }))
		return Failure(Error);
	std::error_code Ec;
	std::filesystem::remove_all(Extract, Ec);
	SetStatus("Launching BestClient-Malinka...");
	const pid_t Child = fork();
	if(Child == 0)
	{
		if(chdir(pArgs->InstallDir.c_str()) != 0)
			_exit(1);
		char *apArgv[] = {pArgs->ExePath.data(), nullptr};
		execv(pArgs->ExePath.c_str(), apArgv);
		_exit(1);
	}
	if(Child < 0)
		return Failure("Update installed, but launching the client failed. Start it manually.");
	SDL_AtomicSet(&g_Done, 1);
	return 0;
}

catch(const std::exception &)
{
	Fail("Update exception. Check rollback backup before restarting.");
	return 1;
}

static SDL_Color LerpColor(SDL_Color A, SDL_Color B, float T)
{
	SDL_Color Out;
	Out.r = (Uint8)(A.r + T * (B.r - A.r));
	Out.g = (Uint8)(A.g + T * (B.g - A.g));
	Out.b = (Uint8)(A.b + T * (B.b - A.b));
	Out.a = 255;
	return Out;
}

static void DrawGradientH(SDL_Renderer *pRenderer, SDL_Rect Rect, SDL_Color C1, SDL_Color C2)
{
	if(Rect.w <= 0)
		return;
	for(int X = 0; X < Rect.w; ++X)
	{
		const float T = Rect.w > 1 ? (float)X / (float)(Rect.w - 1) : 0.0f;
		const SDL_Color C = LerpColor(C1, C2, T);
		SDL_SetRenderDrawColor(pRenderer, C.r, C.g, C.b, 255);
		SDL_Rect Col = {Rect.x + X, Rect.y, 1, Rect.h};
		SDL_RenderFillRect(pRenderer, &Col);
	}
}

static void Render(SDL_Renderer *pRenderer)
{
	SDL_SetRenderDrawColor(pRenderer, C_BG.r, C_BG.g, C_BG.b, 255);
	SDL_RenderClear(pRenderer);

	SDL_Rect TopBar = {0, 0, WND_W, 4};
	DrawGradientH(pRenderer, TopBar, C_GREEN, C_ORANGE);

	{
		const int Scale = 4;
		const int W1 = BcFont::TextWidth("BestClient", Scale);
		BcFont::DrawText(pRenderer, (WND_W - W1) / 2, 14, Scale, "BestClient", C_TITLE);
		const int W2 = BcFont::TextWidth("Updater", Scale);
		BcFont::DrawText(pRenderer, (WND_W - W2) / 2, 48, Scale, "Updater", C_TITLE);
	}

	const int BarL = 40, BarR = WND_W - 80, BarT = 110, BarB = 134;

	SDL_SetRenderDrawColor(pRenderer, C_BAR_BG.r, C_BAR_BG.g, C_BAR_BG.b, 255);
	{
		SDL_Rect Bg = {BarL, BarT, BarR - BarL, BarB - BarT};
		SDL_RenderFillRect(pRenderer, &Bg);
	}

	const int Pct = SDL_AtomicGet(&g_Percent);
	const bool Failed = SDL_AtomicGet(&g_Failed) != 0;
	const int FillW = (BarR - BarL) * Pct / 100;
	if(FillW > 0)
	{
		if(Failed)
		{
			SDL_SetRenderDrawColor(pRenderer, C_ERROR.r, C_ERROR.g, C_ERROR.b, 255);
			SDL_Rect Fill = {BarL, BarT, FillW, BarB - BarT};
			SDL_RenderFillRect(pRenderer, &Fill);
		}
		else
		{
			const SDL_Color FillEnd = LerpColor(C_GREEN, C_ORANGE, Pct / 100.0f);
			SDL_Rect Fill = {BarL, BarT, FillW, BarB - BarT};
			DrawGradientH(pRenderer, Fill, C_GREEN, FillEnd);
		}
	}

	{
		char aBuf[8];
		snprintf(aBuf, sizeof(aBuf), "%d%%", Pct);
		BcFont::DrawText(pRenderer, BarR + 6, BarT + 5, 2, aBuf, Failed ? C_ERROR : C_TITLE);
	}

	{
		const std::string Status = GetStatus();
		BcFont::DrawText(pRenderer, BarL, BarB + 12, 2, Status.c_str(), Failed ? C_ERROR : C_DIM);
	}

	if(Failed)
		BcFont::DrawText(pRenderer, BarL, WND_H - 24, 2, "Press Esc to close.", C_DIM);

	SDL_RenderPresent(pRenderer);
}

int main(int argc, char **argv)
{
	fprintf(stderr, "%s\n", MALINKA_UPDATER_MARKER);
	if(argc != 7)
	{
		fprintf(stderr, "Usage: bestclient-updater <pid> <archive> <install_dir> <exe> <sha256> <version>\n");
		return 1;
	}

	auto pArgs = std::make_unique<SWorkerArgs>();
	uint64_t ParsedPid = 0;
	const char *pPidEnd = argv[1] + std::strlen(argv[1]);
	const auto PidResult = std::from_chars(argv[1], pPidEnd, ParsedPid);
	if(PidResult.ec != std::errc() || PidResult.ptr != pPidEnd ||
		ParsedPid <= 1 || ParsedPid > static_cast<uint64_t>(std::numeric_limits<pid_t>::max()) ||
		ParsedPid == static_cast<uint64_t>(getpid()))
	{
		fprintf(stderr, "Invalid client process ID.\n");
		return 1;
	}
	pArgs->Pid = static_cast<pid_t>(ParsedPid);
	pArgs->Archive = argv[2];
	pArgs->InstallDir = argv[3];
	pArgs->ExePath = argv[4];
	pArgs->ArchiveSha256 = argv[5];
	pArgs->Version = argv[6];

	if(SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

	g_pLock = SDL_CreateMutex();
	if(!g_pLock)
	{
		SDL_Quit();
		return 1;
	}

	SDL_Window *pWindow = SDL_CreateWindow("BestClient-Malinka Updater",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		WND_W, WND_H, SDL_WINDOW_BORDERLESS);
	if(!pWindow)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_DestroyMutex(g_pLock);
		SDL_Quit();
		return 1;
	}

	SDL_Renderer *pRenderer = SDL_CreateRenderer(pWindow, -1, SDL_RENDERER_ACCELERATED);
	if(!pRenderer)
		pRenderer = SDL_CreateRenderer(pWindow, -1, SDL_RENDERER_SOFTWARE);

	if(!pRenderer)
	{
		SDL_DestroyWindow(pWindow);
		SDL_DestroyMutex(g_pLock);
		SDL_Quit();
		return 1;
	}
	auto *pWorkerArgs = pArgs.release();
	SDL_Thread *pThread = SDL_CreateThread(WorkerThreadFn, "bc-updater-worker", pWorkerArgs);
	if(!pThread)
	{
		delete pWorkerArgs;
		SDL_DestroyRenderer(pRenderer);
		SDL_DestroyWindow(pWindow);
		SDL_DestroyMutex(g_pLock);
		SDL_Quit();
		return 1;
	}

	bool Running = true;
	while(Running)
	{
		SDL_Event Event;
		while(SDL_PollEvent(&Event))
		{
			if(Event.type == SDL_QUIT)
				Running = false;
			else if(Event.type == SDL_KEYDOWN && Event.key.keysym.sym == SDLK_ESCAPE)
				Running = false;
		}

		if(SDL_AtomicGet(&g_Done) != 0)
			Running = false;

		Render(pRenderer);
		SDL_Delay(16);
	}

	int WorkerExit = 1;
	SDL_WaitThread(pThread, &WorkerExit); // No worker may access a destroyed UI mutex.
	SDL_DestroyRenderer(pRenderer);
	SDL_DestroyWindow(pWindow);
	SDL_DestroyMutex(g_pLock);
	SDL_Quit();
	return WorkerExit;
}
