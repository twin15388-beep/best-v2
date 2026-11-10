#include "replay_vault.h"

#include "replay_vault_io.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/engine.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/shared/jobs.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>

#include <exception>

class CMalinkaReplayVault::CCacheJob : public IJob
{
	std::shared_ptr<IHttpRequest> m_pArchive;
	std::filesystem::path m_ReplayRoot;
	std::filesystem::path m_CacheRoot;
	std::string m_Revision;

	void Run() override
	{
		try
		{
			if(State() == STATE_ABORTED)
				return;
			unsigned char *pData = nullptr;
			size_t Size = 0;
			m_pArchive->Result(&pData, &Size);
			if(Size < 22 || Size > Malinka::ReplayVault::MAX_ARCHIVE_BYTES)
			{
				str_copy(m_aError, "Vault archive response exceeds limits or is truncated.");
				return;
			}
			const std::string Data(reinterpret_cast<const char *>(pData), Size);
			m_pArchive.reset();
			std::string Error;
			m_Succeeded = Malinka::ReplayVault::CacheArchive(Data, m_Revision, m_ReplayRoot, m_CacheRoot,
				m_Result, Error, [&] { return State() == STATE_ABORTED; });
			if(!m_Succeeded)
				str_copy(m_aError, Error.c_str());
		}
		catch(const std::exception &)
		{
			str_copy(m_aError, "Vault cache failed (allocation/filesystem exception). Old snapshots/user replays were not replaced.");
		}
	}

public:
	// Read results ONLY after STATE_DONE; STATE_ABORTED may still be running.
	Malinka::ReplayVault::SCacheResult m_Result;
	bool m_Succeeded = false;
	char m_aError[256] = "Vault work did not complete.";

	CCacheJob(std::shared_ptr<IHttpRequest> pArchive, std::filesystem::path ReplayRoot,
		std::filesystem::path CacheRoot, std::string Revision) :
		m_pArchive(std::move(pArchive)), m_ReplayRoot(std::move(ReplayRoot)),
		m_CacheRoot(std::move(CacheRoot)), m_Revision(std::move(Revision))
	{
		Abortable(true);
	}
};

namespace
{
std::shared_ptr<IHttpRequest> VaultRequest(const char *pUrl, int64_t Limit, const char *pAccept)
{
	auto pRequest = HttpGet(pUrl);
	pRequest->RequireHttps();
	pRequest->WriteToMemory();
	pRequest->MaxResponseSize(Limit);
	pRequest->Timeout(CTimeout{10000, 120000, 1024, 15});
	pRequest->LogProgress(HTTPLOG::FAILURE);
	pRequest->HeaderString("User-Agent", "BestClient-Malinka-ReplayVault");
	pRequest->HeaderString("Accept", pAccept);
	pRequest->HeaderString("X-GitHub-Api-Version", "2022-11-28");
	return pRequest;
}
}

void CMalinkaReplayVault::Notify(const char *pMessage) const noexcept
{
	try
	{
		if(GameClient())
		{
			if(Console())
				Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "malinka/vault", pMessage);
			GameClient()->m_MalinkaVisuals.PushNotification(SWarning("Replay vault", pMessage));
		}
	}
	catch(...)
	{
		// Notifications are optional, including on allocation failure/shutdown.
	}
}

void CMalinkaReplayVault::CancelSync() noexcept
{
	if(m_pRequest)
		m_pRequest->Abort();
	if(m_pCacheJob)
		m_pCacheJob->Abort();
	m_pRequest.reset();
	m_pCacheJob.reset();
	m_State = EState::IDLE;
	str_copy(m_aStatus, "Vault sync cancelled. Existing replays were not replaced.");
}

void CMalinkaReplayVault::OnShutdown()
{
	CancelSync();
}

void CMalinkaReplayVault::Fail(const char *pMessage) noexcept
{
	CancelSync();
	str_copy(m_aStatus, pMessage);
	Notify(m_aStatus);
}

void CMalinkaReplayVault::StartSync() try
{
	if(Busy())
		return;
	if(!Http() || !Engine() || !Storage())
	{
		Fail("Vault needs initialized HTTP, worker jobs and storage.");
		return;
	}
	if(time_get() < m_NextSyncAllowed)
	{
		Notify("Vault checks are limited to one per minute. Wait before retrying.");
		return;
	}
	m_NextSyncAllowed = time_get() + 60 * time_freq();
	m_Revision.clear();
	m_pRequest = VaultRequest(Malinka::ReplayVault::HEAD_URL, 128, "application/vnd.github.sha");
	m_State = EState::HEAD;
	str_copy(m_aStatus, "Checking krxclient/krx-replays (public GitHub API).");
	Http()->Run(m_pRequest);
}
catch(const std::exception &)
{
	Fail("Cannot start vault sync (allocation/HTTP exception).");
}

void CMalinkaReplayVault::StartArchiveRequest()
{
	const auto Url = std::string(Malinka::ReplayVault::ARCHIVE_URL) + m_Revision;
	m_pRequest = VaultRequest(Url.c_str(), Malinka::ReplayVault::MAX_ARCHIVE_BYTES, "application/zip");
	m_State = EState::ARCHIVE;
	str_copy(m_aStatus, "Downloading a pinned community snapshot (limit 32 MiB).");
	Http()->Run(m_pRequest);
}

void CMalinkaReplayVault::OnUpdate() try
{
	const bool AutoSync = g_Config.m_MalinkaTasReplayVaultAutoSync != 0;
	if(AutoSync && !m_WasAutoSync)
		StartSync(); // Startup, or an explicit off-to-on user setting change.
	m_WasAutoSync = AutoSync;
	if(m_State == EState::IDLE)
		return;
	if(m_State == EState::CACHE)
	{
		if(!m_pCacheJob)
		{
			Fail("Vault cache job is missing.");
			return;
		}
		const auto State = m_pCacheJob->State();
		if(State == IJob::STATE_ABORTED)
		{
			// Do not read job results after abort: its worker may still run.
			CancelSync();
			return;
		}
		if(State != IJob::STATE_DONE)
			return;
		if(!m_pCacheJob->m_Succeeded)
		{
			// Keep the owned job alive until Fail has copied its error text.
			const auto pFinishedJob = m_pCacheJob;
			Fail(pFinishedJob->m_aError);
			return;
		}
		str_format(m_aStatus, sizeof(m_aStatus), "%s %d raw vault files. Reload list; imports still need validation.",
			m_pCacheJob->m_Result.m_AlreadyPresent ? "Verified cached" : "Cached", m_pCacheJob->m_Result.m_Files);
		m_pCacheJob.reset();
		m_State = EState::IDLE;
		Notify(m_aStatus);
		return;
	}
	if(!m_pRequest)
	{
		Fail("Vault request is missing. No cache was published.");
		return;
	}
	if(!m_pRequest->Done())
		return;
	if(m_pRequest->State() != EHttpState::DONE)
	{
		// StatusCode()/Result() are legal only in DONE, not ERROR/ABORTED.
		Fail("Vault HTTP failed/cancelled. Rate limit/network/TLS may be unavailable; no cache published.");
		return;
	}
	if(m_pRequest->StatusCode() != 200)
	{
		char aError[160];
		str_format(aError, sizeof(aError), "Unexpected vault HTTP status %d. No cache published.", m_pRequest->StatusCode());
		Fail(aError);
		return;
	}
	if(m_State == EState::HEAD)
	{
		unsigned char *pData = nullptr;
		size_t Size = 0;
		m_pRequest->Result(&pData, &Size);
		if(Size > 128 || !Malinka::ReplayVault::ReadRevision(std::string_view(reinterpret_cast<const char *>(pData), Size), m_Revision))
		{
			Fail("GitHub did not return a supported 40-digit commit SHA. No archive URL was accepted.");
			return;
		}
		StartArchiveRequest();
	}
	else if(m_State == EState::ARCHIVE)
	{
		char aCachePath[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "malinka/vault-cache", aCachePath, sizeof(aCachePath));
		m_pCacheJob = std::make_shared<CCacheJob>(m_pRequest, Malinka::Replay::Root(Storage()),
			std::filesystem::u8path(aCachePath), m_Revision);
		m_pRequest.reset();
		m_State = EState::CACHE;
		str_copy(m_aStatus, "Checking ZIP paths/limits/CRC and publishing a separate snapshot.");
		Engine()->AddJob(m_pCacheJob);
	}
}
catch(const std::exception &)
{
	Fail("Vault processing failed (allocation/filesystem/HTTP exception).");
}
