// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#include "simulation_step.h"
#include <base/time.h>
#include <algorithm>
#include "misc.h"
#include "capabilities.h"

#include <base/math.h>
#include <base/str.h>

#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/components/chat.h>
#include <game/client/components/emoticon.h>
#include <game/client/components/voting.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/teamscore.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>
#include <memory>
#include <limits>
#include <exception>
#include <game/collision.h>

namespace
{
constexpr int64_t EMOTE_SPAM_INTERVAL_MS = 10;
constexpr int64_t MASS_MENTION_INTERVAL_MS = 5000;
constexpr int64_t AUTO_TEAM_INTERVAL_MS = 2000;
constexpr int64_t AUTO_VOTE_KICK_INTERVAL_MS = 9000;

int64_t MsToTimeFreq(int64_t Ms)
{
	return Ms * time_freq() / 1000;
}

bool IsUsablePlayer(const CGameClient *pGameClient, int ClientId)
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	if(ClientId == pGameClient->m_Snap.m_LocalClientId)
		return false;
	if(!pGameClient->m_Snap.m_aCharacters[ClientId].m_Active || !pGameClient->m_Snap.m_apPlayerInfos[ClientId])
		return false;
	return pGameClient->m_Snap.m_apPlayerInfos[ClientId]->m_Team != TEAM_SPECTATORS;
}

void ToLowerAscii(const char *pSrc, char *pDst, int DstSize)
{
	if(DstSize <= 0)
		return;

	int Cursor = 0;
	while(pSrc[Cursor] != '\0' && Cursor < DstSize - 1)
	{
		const char Chr = pSrc[Cursor];
		pDst[Cursor] = Chr >= 'A' && Chr <= 'Z' ? Chr - 'A' + 'a' : Chr;
		++Cursor;
	}
	pDst[Cursor] = '\0';
}

bool ContainsAnyKeyword(const char *pText, const std::array<const char *, 10> &aKeywords)
{
	char aLower[128];
	ToLowerAscii(pText, aLower, sizeof(aLower));
	for(const char *pKeyword : aKeywords)
	{
		if(str_find_nocase(aLower, pKeyword) != nullptr)
			return true;
	}
	return false;
}

bool IsFrozenLocalCharacter(CGameClient *pGameClient)
{
	const int LocalClientId = pGameClient->m_Snap.m_LocalClientId;
	if(LocalClientId < 0 || !pGameClient->m_Snap.m_aCharacters[LocalClientId].m_Active)
		return false;

	CCharacter *pLocalCharacter = pGameClient->m_PredictedWorld.GetCharacterById(LocalClientId);
	return pLocalCharacter != nullptr && (pLocalCharacter->m_FreezeTime > 0 || pLocalCharacter->Core()->m_DeepFrozen || pLocalCharacter->Core()->m_LiveFrozen);
}
}

void CMalinkaMisc::OnConsoleInit()
{
	Console()->Register("malinka_status", "", CFGFLAG_CLIENT, [](IConsole::IResult *, void *pUser) {
		auto *pSelf = static_cast<CMalinkaMisc *>(pUser);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "malinka", "Experimental adaptation, not official KRX or a complete 1.32 port; see docs/MALINKA_STATUS.md.");
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "malinka", "Insert hides tabs until full process restart, not reconnect; functions remain active. Remote badge authorization is unavailable.");
		size_t Count;
		const auto *pControls = Malinka::UnavailableControls(Count);
		for(size_t Index = 0; Index < Count; ++Index)
		{
			char aLine[512];
			str_format(aLine, sizeof(aLine), "%s: %s", pControls[Index].m_pConfigName, pControls[Index].m_pReason);
			pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "malinka/unavailable", aLine);
		}
	}, this, "List unavailable Malinka controls and build limitations");
}

void CMalinkaMisc::OnReset()
{
	const int64_t Now = time_get();
	m_NextAutoTeamTry = Now;
	m_LastEmoteSpam = Now;
	m_LastMassMentionSpam = Now;
	m_LastAutoVoteKick = Now;
	m_LastIdSteal = Now;
	m_LastChatRepeat = 0;
	m_WasFrozen = false;
	std::fill(std::begin(m_aModDetected), std::end(m_aModDetected), false);
	std::fill(std::begin(m_aWarnDetected), std::end(m_aWarnDetected), false);
}

CMalinkaMisc::~CMalinkaMisc()
{
	StopBenchmark(); // join BEFORE result buffers/mutex are destroyed
}

void CMalinkaMisc::StopBenchmark()
{
	if(m_BenchmarkThread.joinable())
	{
		m_BenchmarkThread.request_stop();
		m_BenchmarkThread.join();
	}
	m_BenchmarkRunning = false;
	m_BenchmarkNotificationReady = false;
}

void CMalinkaMisc::OnShutdown()
{
	StopBenchmark();
}

void CMalinkaMisc::OnStateChange(int NewState, int OldState)
{
	if(g_Config.m_MalinkaRandomTimeoutSeed && NewState == IClient::STATE_CONNECTING && OldState < IClient::STATE_CONNECTING)
		Client()->GenerateTimeoutSeed();

	if(NewState != IClient::STATE_ONLINE)
	{
		StopBenchmark();
		m_BenchmarkToggleLatch = false;
		g_Config.m_MalinkaPredictionBenchmark = 0;
		OnReset();
		return;
	}

	const int64_t Now = time_get();
	m_NextAutoTeamTry = Now;
	m_LastEmoteSpam = Now;
	m_LastMassMentionSpam = Now;
	m_LastAutoVoteKick = Now;
	m_LastIdSteal = Now;
}

void CMalinkaMisc::OnUpdate()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;

	const int64_t Now = time_get();
	UpdateDummyFly();
	UpdateAutoTeam(Now);
	UpdateEmoteSpam(Now);
	UpdateAutoVoteKick(Now);
	UpdateMassMentionSpam(Now);
	UpdateIdStealer(Now);
	UpdateKillOnFreeze();
	UpdateModDetector();
	UpdatePredictionBenchmark();
}

void CMalinkaMisc::UpdateAutoTeam(int64_t Now)
{
	if(!g_Config.m_MalinkaAutoTeam || GameClient()->m_Snap.m_LocalClientId < 0)
		return;
	if(GameClient()->m_Snap.m_apPlayerInfos[GameClient()->m_Snap.m_LocalClientId] &&
		GameClient()->m_Snap.m_apPlayerInfos[GameClient()->m_Snap.m_LocalClientId]->m_Team == TEAM_SPECTATORS)
		return;
	if(GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) != TEAM_FLOCK)
		return;
	if(Now < m_NextAutoTeamTry)
		return;

	char aCommand[32];
	str_format(aCommand, sizeof(aCommand), "/team %d", std::clamp(g_Config.m_MalinkaAutoTeamPreferred, 1, 63));
	GameClient()->m_Chat.SendChat(0, aCommand);
	m_NextAutoTeamTry = Now + MsToTimeFreq(AUTO_TEAM_INTERVAL_MS);
}

void CMalinkaMisc::UpdateDummyFly()
{
	if(Malinka::UnavailableReason(&g_Config.m_MalinkaDummyFly))
		return;
	if(g_Config.m_MalinkaDummyFly)
		g_Config.m_ClDummyHammer = 1;
}

void CMalinkaMisc::UpdateEmoteSpam(int64_t Now)
{
	if(!g_Config.m_MalinkaEmoteSpam || Now < m_LastEmoteSpam + MsToTimeFreq(EMOTE_SPAM_INTERVAL_MS))
		return;

	GameClient()->m_Emoticon.Emote(rand() % NUM_EMOTICONS);
	m_LastEmoteSpam = Now;
}

void CMalinkaMisc::UpdateAutoVoteKick(int64_t Now)
{
	if(!g_Config.m_MalinkaAutoVoteKick || GameClient()->m_Voting.IsVoting() || Now < m_LastAutoVoteKick + MsToTimeFreq(AUTO_VOTE_KICK_INTERVAL_MS))
		return;

	int TargetId = g_Config.m_MalinkaAutoVoteKickMode == 1 ? g_Config.m_MalinkaAutoVoteKickTarget : ChooseIdentityTarget(false);
	if(!IsUsablePlayer(GameClient(), TargetId))
		return;

	GameClient()->m_Voting.CallvoteKick(TargetId, g_Config.m_MalinkaAutoVoteKickReason, false);
	m_LastAutoVoteKick = Now;
}

void CMalinkaMisc::BuildMentionSpam(char *pBuf, int BufSize) const
{
	pBuf[0] = '\0';

	std::vector<int> vCandidates;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(IsUsablePlayer(GameClient(), ClientId) && GameClient()->m_aClients[ClientId].m_aName[0] != '\0')
			vCandidates.push_back(ClientId);
	}

	const int MentionCount = std::min<int>(3, vCandidates.size());
	for(int Index = 0; Index < MentionCount; ++Index)
	{
		const int PickedIndex = rand() % vCandidates.size();
		const int ClientId = vCandidates[PickedIndex];
		if(pBuf[0] != '\0')
			str_append(pBuf, " ", BufSize);
		str_append(pBuf, GameClient()->m_aClients[ClientId].m_aName, BufSize);
		vCandidates.erase(vCandidates.begin() + PickedIndex);
	}

	if(pBuf[0] != '\0')
		str_append(pBuf, " ", BufSize);
	// No third-party advertisement is appended to user chat.
}

void CMalinkaMisc::UpdateMassMentionSpam(int64_t Now)
{
	if(!g_Config.m_MalinkaMassMentionSpam || Now < m_LastMassMentionSpam + MsToTimeFreq(MASS_MENTION_INTERVAL_MS))
		return;

	char aBuf[256];
	BuildMentionSpam(aBuf, sizeof(aBuf));
	if(aBuf[0] == '\0')
		return;

	GameClient()->m_Chat.SendChat(0, aBuf);
	m_LastMassMentionSpam = Now;
}

int CMalinkaMisc::ChooseIdentityTarget(bool Closest) const
{
	std::vector<int> vCandidates;
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(IsUsablePlayer(GameClient(), ClientId))
			vCandidates.push_back(ClientId);
	}

	if(vCandidates.empty())
		return -1;

	if(!Closest)
		return vCandidates[rand() % vCandidates.size()];

	const CCharacter *pLocalCharacter = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_Snap.m_LocalClientId);
	if(pLocalCharacter == nullptr)
		return vCandidates[rand() % vCandidates.size()];

	int ClosestClientId = -1;
	float ClosestDistance = 1e9f;
	for(const int ClientId : vCandidates)
	{
		const float Distance = distance(pLocalCharacter->GetPos(), GameClient()->m_aClients[ClientId].m_RenderPos);
		if(Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestClientId = ClientId;
		}
	}

	return ClosestClientId;
}

void CMalinkaMisc::CopyIdentityFrom(int ClientId)
{
	if(!IsUsablePlayer(GameClient(), ClientId))
		return;

	const auto &Target = GameClient()->m_aClients[ClientId];
	bool Dirty = false;

	if(g_Config.m_MalinkaIdStealerName)
	{
		str_copy(g_Config.m_PlayerName, Target.m_aName);
		Dirty = true;
	}
	if(g_Config.m_MalinkaIdStealerClan)
	{
		str_copy(g_Config.m_PlayerClan, Target.m_aClan);
		Dirty = true;
	}
	if(g_Config.m_MalinkaIdStealerSkin)
	{
		str_copy(g_Config.m_ClPlayerSkin, Target.m_aSkinName);
		g_Config.m_ClPlayerUseCustomColor = Target.m_UseCustomColor;
		g_Config.m_ClPlayerColorBody = Target.m_ColorBody;
		g_Config.m_ClPlayerColorFeet = Target.m_ColorFeet;
		Dirty = true;
	}
	if(g_Config.m_MalinkaIdStealerFlag)
	{
		g_Config.m_PlayerCountry = Target.m_Country;
		Dirty = true;
	}
	if(g_Config.m_MalinkaIdStealerEmote)
	{
		g_Config.m_ClPlayerDefaultEyes = std::clamp(Target.m_Emoticon, 0, NUM_EMOTES - 1);
		Dirty = true;
	}

	if(Dirty)
		GameClient()->SendInfo(false);
}

void CMalinkaMisc::UpdateIdStealer(int64_t Now)
{
	if(!g_Config.m_MalinkaIdStealer || Now < m_LastIdSteal + MsToTimeFreq((int64_t)g_Config.m_MalinkaIdStealerSpeed * 1000))
		return;

	const int TargetId = ChooseIdentityTarget(g_Config.m_MalinkaIdStealerClosest != 0);
	if(TargetId >= 0)
		CopyIdentityFrom(TargetId);
	m_LastIdSteal = Now;
}

void CMalinkaMisc::UpdateKillOnFreeze()
{
	const bool Frozen = IsFrozenLocalCharacter(GameClient());
	if(g_Config.m_MalinkaKillOnFreeze && Frozen && !m_WasFrozen)
		GameClient()->m_Chat.SendChat(0, "/kill");
	m_WasFrozen = Frozen;
}

bool CMalinkaMisc::DetectPlayerByName(int ClientId, bool WarnOnly) const
{
	if(!IsUsablePlayer(GameClient(), ClientId))
		return false;

	static const std::array<const char *, 10> s_aModKeywords = {
		"moderator",
		"[mod]",
		"[admin]",
		" admin",
		"helper",
		"staff",
		"guard",
		"police",
		"mod_",
		"mod "};
	static const std::array<const char *, 10> s_aWarnKeywords = {
		"report",
		"screenshare",
		"record",
		"rec ",
		"demo",
		"clip",
		"cheater",
		"ban",
		"proof",
		"ticket"};

	const auto &ClientData = GameClient()->m_aClients[ClientId];
	return WarnOnly ? (ContainsAnyKeyword(ClientData.m_aName, s_aWarnKeywords) || ContainsAnyKeyword(ClientData.m_aClan, s_aWarnKeywords)) :
			  (ContainsAnyKeyword(ClientData.m_aName, s_aModKeywords) || ContainsAnyKeyword(ClientData.m_aClan, s_aModKeywords));
}

void CMalinkaMisc::UpdateModDetector()
{
	if(Malinka::UnavailableReason(&g_Config.m_MalinkaModDetector))
		return;
	if(!g_Config.m_MalinkaModDetector)
		return;

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
	{
		if(!IsUsablePlayer(GameClient(), ClientId))
			continue;

		if(g_Config.m_MalinkaModDetectorNames && !m_aModDetected[ClientId] && DetectPlayerByName(ClientId, false))
		{
			m_aModDetected[ClientId] = true;

			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "%s joined or is present (id %d)", GameClient()->m_aClients[ClientId].m_aName, ClientId);
			Client()->AddWarning(SWarning("Moderator Detected", aBuf));
			if(g_Config.m_MalinkaModDetectorLeave)
			{
				Client()->Disconnect();
				return;
			}
		}

		if(g_Config.m_MalinkaModDetectorWarn && !m_aWarnDetected[ClientId] && DetectPlayerByName(ClientId, true))
		{
			m_aWarnDetected[ClientId] = true;

			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "%s looks suspicious (id %d)", GameClient()->m_aClients[ClientId].m_aName, ClientId);
			Client()->AddWarning(SWarning("Warn Detected", aBuf));
			if(g_Config.m_MalinkaModDetectorWarnLeave)
			{
				Client()->Disconnect();
				return;
			}
		}
	}
}

void CMalinkaMisc::QueueBenchmarkResult(const char *pTitle, const char *pMessage)
{
	std::scoped_lock Lock(m_BenchmarkResultMutex);
	str_copy(m_aBenchmarkTitle, pTitle, sizeof(m_aBenchmarkTitle));
	str_copy(m_aBenchmarkMessage, pMessage, sizeof(m_aBenchmarkMessage));
	m_BenchmarkNotificationReady = true;
}

void CMalinkaMisc::StartPredictionBenchmark() try
{
	if(m_BenchmarkRunning.exchange(true))
		return;
	if(m_BenchmarkThread.joinable())
		m_BenchmarkThread.join();
	// Capture all live pointers on the main thread. A benchmark worker must never
	// read the live map, tuning, global config, ego-tile editor, or client arrays.
	auto pInitial = std::make_unique<CGameWorld>();
	if(!pInitial->CopyWorldIndependent(&GameClient()->m_PredictedWorld))
	{
		m_BenchmarkRunning = false;
		QueueBenchmarkResult("Prediction benchmark", "A live bounded map/world is required.");
		return;
	}
	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(!pInitial->GetCharacterById(LocalId) || pInitial->m_GameTick > std::numeric_limits<int>::max() - 256)
	{
		m_BenchmarkRunning = false;
		QueueBenchmarkResult("Prediction benchmark", "A local character is required.");
		return;
	}
	const int Runs = std::clamp(g_Config.m_MalinkaPredictionBenchmarkRuns, 1, 1000);
	const int64_t TileCount = static_cast<int64_t>(Collision()->GetWidth()) * Collision()->GetHeight();
	const int MemoryBound = static_cast<int>(std::max<int64_t>(1, 16000000 / std::max<int64_t>(1, TileCount)));
	const int Threads = std::clamp(g_Config.m_MalinkaPredictionBenchmarkThreads, 1, std::min({16, MemoryBound, Runs}));
	std::vector<std::unique_ptr<CGameWorld>> vpWorlds;
	for(int Index = 0; Index < Threads; ++Index)
	{
		auto pWorld = std::make_unique<CGameWorld>();
		if(!pWorld->CopyWorldIndependent(pInitial.get()))
		{
			m_BenchmarkRunning = false;
			QueueBenchmarkResult("Prediction benchmark", "Cannot allocate independent benchmark worlds.");
			return;
		}
		vpWorlds.push_back(std::move(pWorld));
	}
	m_BenchmarkThread = std::jthread([this, Runs, Threads, LocalId, pInitial = std::move(pInitial), vpWorlds = std::move(vpWorlds)](std::stop_token StopToken) mutable {
		using clock = std::chrono::steady_clock;
		const auto Start = clock::now();
		const auto Deadline = Start + std::chrono::seconds(15);
		std::atomic<uint64_t> TotalTicks = 0;
		std::atomic<bool> Limited = false;
		std::atomic<bool> Failed = false;
		const auto Worker = [&](int ThreadIndex) noexcept {
			try
			{
			auto &World = *vpWorlds[ThreadIndex];
			uint64_t Ticks = 0;
			for(int Run = ThreadIndex; Run < Runs && !StopToken.stop_requested() && !Failed.load(); Run += Threads)
			{
				if(!World.CopyWorldIndependent(pInitial.get()))
				{
					Failed = true;
					break;
				}
				World.m_WorldConfig.m_PredictEvents = false;
				uint32_t Seed = 0x9e3779b9u ^ static_cast<uint32_t>(Run);
				for(int Step = 0; Step < 256 && !StopToken.stop_requested() && !Failed.load(); ++Step)
				{
					if(clock::now() >= Deadline)
					{
						Limited = true;
						break;
					}
					CCharacter *pChar = World.GetCharacterById(LocalId);
					if(!pChar)
						break;
					Seed = Seed * 1664525u + 1013904223u;
					CNetObj_PlayerInput Input{};
					Input.m_TargetX = 1000;
					Input.m_Direction = static_cast<int>((Seed >> 16) % 3) - 1;
					Input.m_Jump = ((Seed >> 8) & 31) == 0;
					Input.m_PlayerFlags = PLAYERFLAG_PLAYING;
					if(!Malinka::TickSimulation(World, {{LocalId, Input}}))
					{
						Failed = true;
						break;
					}
					++Ticks;
				}
				if(Limited)
					break;
			}
			TotalTicks.fetch_add(Ticks);
			}
			catch(...)
			{
				Failed = true; // Exceptions cannot escape a std::thread entry point.
			}
		};
		std::vector<std::jthread> vWorkers;
		try
		{
			vWorkers.reserve(Threads - 1);
			for(int Index = 1; Index < Threads; ++Index)
				vWorkers.emplace_back([&, Index] { Worker(Index); });
			Worker(0);
			for(auto &Thread : vWorkers)
				Thread.join();
		}
		catch(const std::exception &)
		{
			Failed = true;
			// Cancel and join successfully created workers before captures go away.
			for(auto &Thread : vWorkers)
				if(Thread.joinable())
					Thread.join();
			Failed = true;
		}
		if(!StopToken.stop_requested())
		{
			char aMessage[256];
			const double Ms = std::chrono::duration<double, std::milli>(clock::now() - Start).count();
			str_format(aMessage, sizeof(aMessage), "%llu actual world ticks, %d workers, %.2f ms, %.0f ticks/s%s%s. Not a gameplay correctness test.",
				static_cast<unsigned long long>(TotalTicks.load()), Threads, Ms, Ms > 0.0 ? TotalTicks.load() * 1000.0 / Ms : 0.0,
				Limited ? " (15s limit)" : "", Failed ? " (worker/copy failure)" : "");
			QueueBenchmarkResult("Prediction benchmark", aMessage);
		}
		m_BenchmarkRunning = false;
	});
}
catch(const std::exception &)
{
	m_BenchmarkRunning = false;
	QueueBenchmarkResult("Prediction benchmark", "Cannot allocate or launch bounded benchmark workers.");
}

void CMalinkaMisc::UpdatePredictionBenchmark()
{
	if(g_Config.m_MalinkaPredictionBenchmark && !m_BenchmarkToggleLatch && !m_BenchmarkRunning)
		StartPredictionBenchmark();
	if(!g_Config.m_MalinkaPredictionBenchmark && m_BenchmarkToggleLatch && m_BenchmarkThread.joinable())
		m_BenchmarkThread.request_stop();
	m_BenchmarkToggleLatch = g_Config.m_MalinkaPredictionBenchmark != 0;

	if(m_BenchmarkNotificationReady.exchange(false))
	{
		char aTitle[128];
		char aMessage[256];
		{
			std::scoped_lock Lock(m_BenchmarkResultMutex);
			str_copy(aTitle, m_aBenchmarkTitle, sizeof(aTitle));
			str_copy(aMessage, m_aBenchmarkMessage, sizeof(aMessage));
		}
		Client()->AddWarning(SWarning(aTitle, aMessage));
		g_Config.m_MalinkaPredictionBenchmark = 0;
		m_BenchmarkToggleLatch = false;
	}
}

void CMalinkaMisc::BuildAlternatingCase(char *pBuf, int BufSize, const char *pSrc) const
{
	pBuf[0] = '\0';
	bool Upper = true;
	for(const char *pChr = pSrc; *pChr != '\0'; ++pChr)
	{
		char aChar[2] = {*pChr, '\0'};
		if(*pChr >= 'a' && *pChr <= 'z')
			aChar[0] = Upper ? (*pChr - 'a' + 'A') : *pChr;
		else if(*pChr >= 'A' && *pChr <= 'Z')
			aChar[0] = Upper ? *pChr : (*pChr - 'A' + 'a');

		if((*pChr >= 'a' && *pChr <= 'z') || (*pChr >= 'A' && *pChr <= 'Z'))
			Upper = !Upper;

		str_append(pBuf, aChar, BufSize);
	}
}

void CMalinkaMisc::OnMessage(int MsgType, void *pRawMsg)
{
	if(Client()->State() != IClient::STATE_ONLINE || GameClient()->m_Snap.m_LocalClientId < 0)
		return;

	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		if(!g_Config.m_MalinkaChatRepeater || pMsg->m_ClientId < 0 || pMsg->m_ClientId == GameClient()->m_Snap.m_LocalClientId)
			return;
		if(str_length(pMsg->m_pMessage) < g_Config.m_MalinkaChatRepeaterLength)
			return;
		if(time_get() < m_LastChatRepeat + time_freq())
			return;

		char aBuf[256];
		BuildAlternatingCase(aBuf, sizeof(aBuf), pMsg->m_pMessage);
		GameClient()->m_Chat.SendChat(0, aBuf);
		m_LastChatRepeat = time_get();
	}
	else if(MsgType == NETMSGTYPE_SV_KILLMSG && g_Config.m_MalinkaKillsay)
	{
		CNetMsg_Sv_KillMsg *pMsg = (CNetMsg_Sv_KillMsg *)pRawMsg;
		static const std::array<const char *, 6> s_aDeathsay = {
			"lagged, totally",
			"nt, i guess",
			"that freeze looked personal",
			"reporting my keyboard",
			"unlucky spawn of destiny",
			"i meant to do that"};
		static const std::array<const char *, 6> s_aKillsay = {
			"nice shot",
			"skill issue",
			"good game",
			"too slow",
			"frozen and folded",
			"gg go next"};

		if(pMsg->m_Victim == GameClient()->m_Snap.m_LocalClientId)
			GameClient()->m_Chat.SendChat(0, s_aDeathsay[rand() % s_aDeathsay.size()]);
		else if(pMsg->m_Killer == GameClient()->m_Snap.m_LocalClientId && pMsg->m_Victim != GameClient()->m_Snap.m_LocalClientId)
			GameClient()->m_Chat.SendChat(0, s_aKillsay[rand() % s_aKillsay.size()]);
	}
}
