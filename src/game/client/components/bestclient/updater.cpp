/* Copyright © 2026 BestProject Team */
#include "updater.h"

#include <base/process.h>
#include <base/hash.h>
#include <game/client/components/malinka/build_identity.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/client/updater.h>
#include <engine/config.h>
#include <engine/external/json-parser/json.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/client/components/bestclient/version.h>
#include <game/client/gameclient.h>
#include <game/version.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static constexpr const char *GITHUB_RELEASES_URL = Malinka::Build::RELEASES_URL;
static constexpr const char *GITHUB_LATEST_RELEASE_URL = Malinka::Build::LATEST_URL;
static constexpr const char *UPDATE_ARCHIVE_PATH = "update/malinka-release.zip";

static bool ValidMalinkaVersion(const char *pVersion)
{
	if(!pVersion || str_length(pVersion) > 32)
		return false;
	int Dots = 0, Digits = 0;
	for(const char *p = pVersion; *p; ++p)
	{
		if(*p == '.')
		{
			if(Digits < 1 || Digits > 6)
				return false;
			++Dots;
			Digits = 0;
		}
		else if(*p >= '0' && *p <= '9')
			++Digits;
		else
			return false;
	}
	return Dots == 2 && Digits >= 1 && Digits <= 6;
}

static bool HasUpdaterMarker(const char *pPath)
{
	std::ifstream In(std::filesystem::u8path(pPath), std::ios::binary);
	std::string Window;
	char aBuffer[65536];
	for(int Blocks = 0; In && Blocks < 1024; ++Blocks)
	{
		In.read(aBuffer, sizeof(aBuffer));
		Window.append(aBuffer, static_cast<size_t>(In.gcount()));
		if(Window.find(MALINKA_UPDATER_MARKER) != std::string::npos)
			return true;
		if(Window.size() > 128)
			Window.erase(0, Window.size() - 128);
	}
	return false;
}

static void BuildGitHubReleasesUrl(char *pBuf, int BufSize)
{
	str_format(pBuf, BufSize, "%s&t=%lld", GITHUB_RELEASES_URL, (long long)time_timestamp());
}

static std::string ToLowerAscii(const char *pStr)
{
	std::string Lower;
	if(!pStr)
		return Lower;

	for(const unsigned char *p = reinterpret_cast<const unsigned char *>(pStr); *p != '\0'; ++p)
		Lower.push_back(static_cast<char>(std::tolower(*p)));
	return Lower;
}

static const char *GetReleaseVersionString(const json_value *pJson)
{
	if(!pJson || pJson->type != json_object || json_boolean_get(json_object_get(pJson, "draft")) || json_boolean_get(json_object_get(pJson, "prerelease")))
		return nullptr;
	const char *pTag = json_string_get(json_object_get(pJson, "tag_name"));
	if(!pTag || !str_startswith(pTag, "malinka-v") || !ValidMalinkaVersion(pTag + 9))
		return nullptr;
	return pTag + 9;
}

static void NormalizeVersionString(const char *pVersion, char *pBuf, int BufSize)
{
	if(BufSize <= 0)
		return;

	if(!pVersion)
	{
		pBuf[0] = '\0';
		return;
	}

	while(*pVersion != '\0' && std::isspace(static_cast<unsigned char>(*pVersion)))
		++pVersion;

	if((pVersion[0] == 'v' || pVersion[0] == 'V') && std::isdigit(static_cast<unsigned char>(pVersion[1])))
		++pVersion;

	str_copy(pBuf, pVersion, BufSize);
}

static std::vector<int> ExtractVersionNumbers(const char *pVersion)
{
	std::vector<int> vNumbers;
	if(!pVersion)
		return vNumbers;

	int Current = -1;
	for(const unsigned char *p = reinterpret_cast<const unsigned char *>(pVersion); *p != '\0'; ++p)
	{
		if(std::isdigit(*p))
		{
			if(Current < 0)
				Current = 0;
			Current = std::min(999999, Current) * 10 + (*p - '0');
		}
		else if(Current >= 0)
		{
			vNumbers.push_back(Current);
			Current = -1;
		}
	}

	if(Current >= 0)
		vNumbers.push_back(Current);

	return vNumbers;
}

static int CompareVersionStrings(const char *pLeft, const char *pRight)
{
	char aLeftNormalized[64];
	char aRightNormalized[64];
	NormalizeVersionString(pLeft, aLeftNormalized, sizeof(aLeftNormalized));
	NormalizeVersionString(pRight, aRightNormalized, sizeof(aRightNormalized));

	const std::vector<int> vLeft = ExtractVersionNumbers(aLeftNormalized);
	const std::vector<int> vRight = ExtractVersionNumbers(aRightNormalized);
	const size_t Num = std::max(vLeft.size(), vRight.size());
	for(size_t i = 0; i < Num; ++i)
	{
		const int Left = i < vLeft.size() ? vLeft[i] : 0;
		const int Right = i < vRight.size() ? vRight[i] : 0;
		if(Left < Right)
			return -1;
		if(Left > Right)
			return 1;
	}

	return str_comp_nocase(aLeftNormalized, aRightNormalized);
}

static int ScoreArchiveAsset(const char *pAssetName)
{
	const std::string Expected = std::string("bestclient-malinka-") + Malinka::Build::PLATFORM + ".zip";
	return pAssetName && ToLowerAscii(pAssetName) == Expected ? 100 : -1;
}

static bool ParseReleaseObject(const json_value *pJson, char *pVersion, int VersionSize, char *pArchiveName, int ArchiveNameSize, char *pArchiveUrl, int ArchiveUrlSize, char *pSha256, int Sha256Size)
{
	if(!pJson || pJson->type != json_object)
		return false;

	const char *pReleaseVersion = GetReleaseVersionString(pJson);
	if(!pReleaseVersion)
		return false;

	const json_value *pAssets = json_object_get(pJson, "assets");
	if(!pAssets || pAssets->type != json_array)
		return false;

	int BestScore = -1;
	char aBestName[128] = "";
	char aBestUrl[2048] = "";
	char aBestSha256[65] = "";

	for(int i = 0; i < json_array_length(pAssets); ++i)
	{
		const json_value *pAsset = json_array_get(pAssets, i);
		if(!pAsset || pAsset->type != json_object)
			continue;

		const char *pName = json_string_get(json_object_get(pAsset, "name"));
		const char *pUrl = json_string_get(json_object_get(pAsset, "browser_download_url"));
		const char *pDigest = json_string_get(json_object_get(pAsset, "digest"));
		const int Score = ScoreArchiveAsset(pName);
		const std::string ExpectedPrefix = std::string(Malinka::Build::DOWNLOAD_PREFIX) + pReleaseVersion + "/";
		SHA256_DIGEST Digest;
		if(!pName || !pUrl || Score < 0 || Score < BestScore || str_length(pUrl) >= 2048 ||
			std::string(pUrl) != ExpectedPrefix + pName || !pDigest || !str_startswith(pDigest, "sha256:") ||
			str_length(pDigest) != 71 || sha256_from_str(&Digest, pDigest + 7) != 0)
			continue;

		BestScore = Score;
		str_copy(aBestName, pName, sizeof(aBestName));
		str_copy(aBestUrl, pUrl, sizeof(aBestUrl));
		str_copy(aBestSha256, pDigest + 7, sizeof(aBestSha256));
	}

	if(BestScore < 0)
		return false;

	str_copy(pVersion, pReleaseVersion, VersionSize);
	str_copy(pArchiveName, aBestName, ArchiveNameSize);
	str_copy(pArchiveUrl, aBestUrl, ArchiveUrlSize);
	str_copy(pSha256, aBestSha256, Sha256Size);
	return true;
}

static bool ParseLatestRelease(json_value *pJson, char *pVersion, int VersionSize, char *pArchiveName, int ArchiveNameSize, char *pArchiveUrl, int ArchiveUrlSize, char *pSha256, int Sha256Size)
{
	if(!pJson)
		return false;

	if(pJson->type == json_object)
		return ParseReleaseObject(pJson, pVersion, VersionSize, pArchiveName, ArchiveNameSize, pArchiveUrl, ArchiveUrlSize, pSha256, Sha256Size);

	if(pJson->type == json_array)
	{
		const json_value *pBestRelease = nullptr;
		char aBestVersion[64] = "";
		for(int i = 0; i < json_array_length(pJson); ++i)
		{
			const json_value *pRelease = json_array_get(pJson, i);
			const char *pReleaseVersion = GetReleaseVersionString(pRelease);
			if(!pReleaseVersion)
				continue;

			char aProbeVersion[64], aProbeName[128], aProbeUrl[2048], aProbeSha[65];
			if(!ParseReleaseObject(pRelease, aProbeVersion, sizeof(aProbeVersion), aProbeName, sizeof(aProbeName), aProbeUrl, sizeof(aProbeUrl), aProbeSha, sizeof(aProbeSha)))
				continue;
			if(!pBestRelease || CompareVersionStrings(pReleaseVersion, aBestVersion) > 0)
			{
				pBestRelease = pRelease;
				str_copy(aBestVersion, pReleaseVersion, sizeof(aBestVersion));
			}
		}

		if(pBestRelease)
			return ParseReleaseObject(pBestRelease, pVersion, VersionSize, pArchiveName, ArchiveNameSize, pArchiveUrl, ArchiveUrlSize, pSha256, Sha256Size);
	}

	return false;
}

static void StripFilename(char *pPath)
{
	if(!pPath)
		return;

	for(int i = str_length(pPath) - 1; i >= 0; --i)
	{
		if(pPath[i] == '/' || pPath[i] == '\\')
		{
			pPath[i] = '\0';
			return;
		}
	}
	pPath[0] = '\0';
}

CUpdater::CUpdater()
{
	m_pClient = nullptr;
	m_pStorage = nullptr;
	m_pHttp = nullptr;

	m_State = CLEAN;
	m_aStatus[0] = '\0';
	m_Percent = 0;
	m_aLatestVersion[0] = '\0';
	m_aArchiveName[0] = '\0';
	m_aArchiveUrl[0] = '\0';
	m_aArchiveSha256[0] = '\0';
	str_copy(m_aArchivePath, UPDATE_ARCHIVE_PATH, sizeof(m_aArchivePath));
}

void CUpdater::Init()
{
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pHttp = Kernel()->RequestInterface<IHttp>();

#if !defined(CONF_HEADLESS_CLIENT) && (defined(CONF_FAMILY_WINDOWS) || defined(CONF_PLATFORM_LINUX))
	m_AutoCheckPending = true;
#endif
}

void CUpdater::SetCurrentState(EUpdaterState NewState)
{
	const CLockScope LockScope(m_Lock);
	m_State = NewState;
}

void CUpdater::SetStatus(const char *pStatus)
{
	const CLockScope LockScope(m_Lock);
	str_copy(m_aStatus, pStatus ? pStatus : "", sizeof(m_aStatus));
}

void CUpdater::SetPercent(int Percent)
{
	const CLockScope LockScope(m_Lock);
	m_Percent = std::clamp(Percent, 0, 100);
}

IUpdater::EUpdaterState CUpdater::GetCurrentState()
{
	const CLockScope LockScope(m_Lock);
	return m_State;
}

void CUpdater::GetCurrentFile(char *pBuf, int BufSize)
{
	const CLockScope LockScope(m_Lock);
	str_copy(pBuf, m_aStatus, BufSize);
}

int CUpdater::GetCurrentPercent()
{
	const CLockScope LockScope(m_Lock);
	return m_Percent;
}

const char *CUpdater::GetLatestVersionString()
{
	return m_aLatestVersion;
}

void CUpdater::ResetTask()
{
	if(m_pCurrentTask)
	{
		m_pCurrentTask->Abort();
		m_pCurrentTask = nullptr;
	}
	m_TaskKind = ETaskKind::NONE;
}

void CUpdater::StartReleaseFetch()
{
	ResetTask();
	SetStatus("Checking Malinka channel (official updates are blocked)");
	SetPercent(0);
	SetCurrentState(IUpdater::GETTING_MANIFEST);

	char aUrl[256];
	BuildGitHubReleasesUrl(aUrl, sizeof(aUrl));
	m_TaskKind = ETaskKind::FETCH_RELEASE;
	m_pCurrentTask = HttpGet(aUrl);
	m_pCurrentTask->MaxResponseSize(4 * 1024 * 1024);
	m_pCurrentTask->HeaderString("Accept", "application/vnd.github+json");
	m_pCurrentTask->HeaderString("User-Agent", CLIENT_NAME);
	m_pCurrentTask->HeaderString("X-GitHub-Api-Version", "2022-11-28");
	m_pCurrentTask->HeaderString("Cache-Control", "no-cache");
	m_pCurrentTask->HeaderString("Pragma", "no-cache");
	m_pCurrentTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pCurrentTask->IpResolve(IPRESOLVE::V4);
	m_pHttp->Run(m_pCurrentTask);
}

void CUpdater::ParseReleaseTask()
{
	json_value *pJson = m_pCurrentTask ? m_pCurrentTask->ResultJson() : nullptr;
	if(!pJson)
	{
		SetStatus("Failed to parse release info");
		SetCurrentState(IUpdater::FAIL);
		return;
	}

	char aVersion[64] = "";
	char aArchiveName[128] = "";
	char aArchiveUrl[2048] = "";
	char aSha256[65] = "";

	const bool Parsed = ParseLatestRelease(pJson, aVersion, sizeof(aVersion), aArchiveName, sizeof(aArchiveName), aArchiveUrl, sizeof(aArchiveUrl), aSha256, sizeof(aSha256));
	json_value_free(pJson);

	if(!Parsed || CompareVersionStrings(aVersion, MALINKA_VERSION) <= 0)
	{
		m_aLatestVersion[0] = '\0';
		m_aArchiveName[0] = '\0';
		m_aArchiveUrl[0] = '\0';
		m_aArchiveSha256[0] = '\0';
		SetStatus("No compatible Malinka update. Official BestClient packages are blocked.");
		SetCurrentState(IUpdater::CLEAN);
		return;
	}

	str_copy(m_aLatestVersion, aVersion, sizeof(m_aLatestVersion));
	str_copy(m_aArchiveName, aArchiveName, sizeof(m_aArchiveName));
	str_copy(m_aArchiveUrl, aArchiveUrl, sizeof(m_aArchiveUrl));
	str_copy(m_aArchiveSha256, aSha256, sizeof(m_aArchiveSha256));
	SetStatus("Update available");
	SetCurrentState(IUpdater::VERSION_AVAILABLE);
}

void CUpdater::StartArchiveDownload()
{
	SHA256_DIGEST ExpectedSha256;
	if(sha256_from_str(&ExpectedSha256, m_aArchiveSha256) != 0 || !ValidMalinkaVersion(m_aLatestVersion))
	{
		SetStatus("Missing trusted archive digest/version; update blocked.");
		SetCurrentState(IUpdater::FAIL);
		return;
	}
	ResetTask();
	str_copy(m_aArchivePath, UPDATE_ARCHIVE_PATH, sizeof(m_aArchivePath));
	m_pStorage->RemoveBinaryFile(m_aArchivePath);

	SetStatus(m_aArchiveName);
	SetPercent(0);
	SetCurrentState(IUpdater::DOWNLOADING);

	m_TaskKind = ETaskKind::DOWNLOAD_ARCHIVE;
	char aAbsolutePath[IO_MAX_PATH_LENGTH];
	m_pStorage->GetBinaryPathAbsolute(m_aArchivePath, aAbsolutePath, sizeof(aAbsolutePath));
	m_pCurrentTask = HttpGetFile(m_aArchiveUrl, m_pStorage, aAbsolutePath, IStorage::TYPE_ABSOLUTE);
	m_pCurrentTask->ExpectSha256(ExpectedSha256);
	m_pCurrentTask->MaxResponseSize(512ll * 1024 * 1024);
	m_pCurrentTask->HeaderString("User-Agent", CLIENT_NAME);
	m_pCurrentTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pCurrentTask->IpResolve(IPRESOLVE::V4);
	m_pHttp->Run(m_pCurrentTask);
}

bool CUpdater::LaunchApplyScriptAndQuit()
{
#if defined(CONF_FAMILY_WINDOWS) || defined(CONF_PLATFORM_LINUX)
	char aArchivePath[IO_MAX_PATH_LENGTH];
	char aUpdaterPath[IO_MAX_PATH_LENGTH];
	char aInstallDir[IO_MAX_PATH_LENGTH];
	char aExePath[IO_MAX_PATH_LENGTH];
	char aPid[32];

	m_pStorage->GetBinaryPathAbsolute(m_aArchivePath, aArchivePath, sizeof(aArchivePath));
	if(!m_pStorage->FileExists(aArchivePath, IStorage::TYPE_ABSOLUTE))
	{
		SetStatus("Downloaded archive missing");
		return false;
	}

#if defined(CONF_FAMILY_WINDOWS)
	m_pStorage->GetBinaryPathAbsolute("bestclient-updater.exe", aUpdaterPath, sizeof(aUpdaterPath));
#else
	m_pStorage->GetBinaryPathAbsolute("bestclient-updater", aUpdaterPath, sizeof(aUpdaterPath));
#endif
	SHA256_DIGEST Expected, Actual;
	if(!HasUpdaterMarker(aUpdaterPath) || sha256_from_str(&Expected, m_aArchiveSha256) != 0 ||
		!m_pStorage->CalculateHashes(aArchivePath, IStorage::TYPE_ABSOLUTE, &Actual) || Actual != Expected)
	{
		SetStatus("Updater identity/archive hash verification failed; restart cancelled.");
		return false;
	}
	m_pStorage->GetBinaryPathAbsolute(PLAT_CLIENT_EXEC, aExePath, sizeof(aExePath));
	str_copy(aInstallDir, aExePath, sizeof(aInstallDir));
	StripFilename(aInstallDir);

	str_format(aPid, sizeof(aPid), "%d", process_id());
	const char *apArguments[] = {aPid, aArchivePath, aInstallDir, aExePath, m_aArchiveSha256, m_aLatestVersion};

	if(process_execute(aUpdaterPath, EShellExecuteWindowState::FOREGROUND, apArguments, std::size(apArguments)) == INVALID_PROCESS)
	{
		SetStatus("Failed to launch updater");
		return false;
	}

	m_pClient->Quit();
	return true;
#else
	SetStatus("Archive updater is only available on Windows and Linux");
	return false;
#endif
}

void CUpdater::CheckForUpdate()
{
	const EUpdaterState State = GetCurrentState();
	if(State == IUpdater::GETTING_MANIFEST || State == IUpdater::DOWNLOADING)
		return;

#if !defined(CONF_FAMILY_WINDOWS) && !defined(CONF_PLATFORM_LINUX)
	if(m_pClient)
		m_pClient->ViewLink(GITHUB_LATEST_RELEASE_URL);
	return;
#endif

	m_aLatestVersion[0] = '\0';
	m_aArchiveName[0] = '\0';
	m_aArchiveUrl[0] = '\0';
	m_aArchiveSha256[0] = '\0';
	StartReleaseFetch();
}

void CUpdater::InitiateUpdate()
{
	const EUpdaterState State = GetCurrentState();
	if(State == IUpdater::GETTING_MANIFEST || State == IUpdater::DOWNLOADING)
		return;

	if((State == IUpdater::VERSION_AVAILABLE || State == IUpdater::FAIL) && m_aArchiveUrl[0] != '\0')
	{
		StartArchiveDownload();
		return;
	}

	CheckForUpdate();
}

bool CUpdater::MainMenuOpen()
{
	IGameClient *pGameClient = Kernel()->RequestInterface<IGameClient>();
	if(!pGameClient)
		return false;
	return static_cast<CGameClient *>(pGameClient)->m_Menus.IsMainMenuOpen();
}

void CUpdater::ApplyUpdateAndRestart()
{
	if(GetCurrentState() != IUpdater::NEED_RESTART || !MainMenuOpen())
		return;

	if(!LaunchApplyScriptAndQuit())
		SetCurrentState(IUpdater::FAIL);
}

void CUpdater::Update()
{
	if(g_Config.m_BcAutoUpdate != 0)
	{
		const EUpdaterState State = GetCurrentState();
		if(State == IUpdater::VERSION_AVAILABLE)
			InitiateUpdate();
		else if(State == IUpdater::NEED_RESTART && MainMenuOpen())
			ApplyUpdateAndRestart();
	}

	if(m_AutoCheckPending && m_pHttp && GetCurrentState() == CLEAN)
	{
		m_AutoCheckPending = false;
		CheckForUpdate();
	}

	if(!m_pCurrentTask)
		return;

	if(!m_pCurrentTask->Done())
	{
		if(GetCurrentState() == IUpdater::DOWNLOADING)
			SetPercent(m_pCurrentTask->Progress());
		return;
	}

	if(m_pCurrentTask->State() != EHttpState::DONE || m_pCurrentTask->StatusCode() >= 400)
	{
		ResetTask();
		SetStatus("Update check failed");
		SetCurrentState(IUpdater::FAIL);
		return;
	}

	if(m_TaskKind == ETaskKind::FETCH_RELEASE)
	{
		ParseReleaseTask();
		ResetTask();
		return;
	}

	if(m_TaskKind == ETaskKind::DOWNLOAD_ARCHIVE)
	{
		ResetTask();
		SetPercent(100);
		SetStatus(m_aArchiveName[0] != '\0' ? m_aArchiveName : "update");
		SetCurrentState(IUpdater::NEED_RESTART);
	}
}
