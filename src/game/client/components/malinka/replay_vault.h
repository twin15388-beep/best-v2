#ifndef GAME_CLIENT_COMPONENTS_MALINKA_REPLAY_VAULT_H
#define GAME_CLIENT_COMPONENTS_MALINKA_REPLAY_VAULT_H

#include <game/client/component.h>

#include <cstdint>
#include <memory>
#include <string>

class IHttpRequest;

// Runs independently of an online TAS session. HTTP is async and ZIP/cache
// work owns only paths/data in an engine job, never game-client raw pointers.
class CMalinkaReplayVault : public CComponent
{
	class CCacheJob;
	enum class EState
	{
		IDLE,
		HEAD,
		ARCHIVE,
		CACHE,
	};
	EState m_State = EState::IDLE;
	std::shared_ptr<IHttpRequest> m_pRequest;
	std::shared_ptr<CCacheJob> m_pCacheJob;
	std::string m_Revision;
	char m_aStatus[160] = "Vault idle. Sync downloads raw community files, not trusted replays.";
	int64_t m_NextSyncAllowed = 0;
	bool m_WasAutoSync = false;

	void Notify(const char *pMessage) const noexcept;
	void Fail(const char *pMessage) noexcept;
	void StartArchiveRequest();

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnUpdate() override;
	void OnShutdown() override;
	void StartSync();
	void CancelSync() noexcept;
	bool Busy() const { return m_State != EState::IDLE; }
	const char *Status() const { return m_aStatus; }
};

#endif
