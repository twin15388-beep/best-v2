// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#include "simulation_step.h"
#include "tas.h"
#include "input_helpers.h"
#include "replay_io.h"
#include "replay_paths.h"

#include <base/hash.h>
#include <engine/map.h>

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/dbg.h>
#include <base/fs.h>
#include <base/io.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <generated/client_data.h>
#include <game/gamecore.h>
#include <game/client/animstate.h>
#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/render.h>
#include <game/client/ui.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

struct STasScreenPixelGrid
{
	float m_ScreenX0;
	float m_ScreenY0;
	float m_PixelSizeX;
	float m_PixelSizeY;
};

int GetTasStepAmount()
{
	return g_Config.m_MalinkaTickControlStep ? 1 : std::max(1, g_Config.m_MalinkaTasTickControlTicks);
}

STasScreenPixelGrid GetTasScreenPixelGrid(const IGraphics *pGraphics)
{
	const CScreenRect Screen = pGraphics->GetScreen();
	const float ScreenX0 = Screen.m_TopLeft.x;
	const float ScreenY0 = Screen.m_TopLeft.y;
	const float ScreenX1 = Screen.m_BottomRight.x;
	const float ScreenY1 = Screen.m_BottomRight.y;

	return {
		ScreenX0,
		ScreenY0,
		(ScreenX1 - ScreenX0) / std::max(1, pGraphics->ScreenWidth()),
		(ScreenY1 - ScreenY0) / std::max(1, pGraphics->ScreenHeight())};
}

vec2 SnapTasPoint(const STasScreenPixelGrid &Grid, vec2 Pos)
{
	return vec2(
		Grid.m_ScreenX0 + roundf((Pos.x - Grid.m_ScreenX0) / Grid.m_PixelSizeX) * Grid.m_PixelSizeX,
		Grid.m_ScreenY0 + roundf((Pos.y - Grid.m_ScreenY0) / Grid.m_PixelSizeY) * Grid.m_PixelSizeY);
}

float SnapTasSpan(float Span, float PixelSize)
{
	return std::max(PixelSize, roundf(Span / PixelSize) * PixelSize);
}

void RenderTasPoints(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, const std::vector<vec2> &vPoints, ColorRGBA Color, float Size)
{
	if(vPoints.empty() || Color.a <= 0.0f)
		return;

	const STasScreenPixelGrid Grid = GetTasScreenPixelGrid(pGraphics);
	const float SizeX = SnapTasSpan(Size, Grid.m_PixelSizeX);
	const float SizeY = SnapTasSpan(Size, Grid.m_PixelSizeY);

	if(Texture.IsValid())
		pGraphics->TextureSet(Texture);
	else
		pGraphics->TextureClear();

	pGraphics->QuadsBegin();
	pGraphics->SetColor(Color);
	for(const vec2 &Point : vPoints)
	{
		const vec2 TopLeft = SnapTasPoint(Grid, Point - vec2(SizeX * 0.5f, SizeY * 0.5f));
		const IGraphics::CQuadItem Quad(TopLeft.x, TopLeft.y, SizeX, SizeY);
		pGraphics->QuadsDrawTL(&Quad, 1);
	}
	pGraphics->QuadsEnd();
}

void RenderTasLines(IGraphics *pGraphics, const std::vector<IGraphics::CLineItem> &vSegments, ColorRGBA Color, float Thickness, float Zoom)
{
	if(vSegments.empty() || Color.a <= 0.0f)
		return;

	const STasScreenPixelGrid Grid = GetTasScreenPixelGrid(pGraphics);
	const float PixelSize = std::max(Grid.m_PixelSizeX, Grid.m_PixelSizeY);
	const float LineWidth = SnapTasSpan(std::max(PixelSize, (0.8f + (Thickness - 1.0f) * 0.5f) * Zoom), PixelSize);

	pGraphics->TextureClear();
	std::vector<IGraphics::CFreeformItem> vQuads;
	vQuads.reserve(vSegments.size());
	for(const auto &Segment : vSegments)
	{
		const vec2 Start = SnapTasPoint(Grid, vec2(Segment.m_X0, Segment.m_Y0));
		const vec2 End = SnapTasPoint(Grid, vec2(Segment.m_X1, Segment.m_Y1));
		const vec2 Diff = End - Start;
		if(length(Diff) < 0.001f)
			continue;
		const vec2 Perp = normalize(vec2(Diff.y, -Diff.x)) * (LineWidth * 0.5f);
		vQuads.emplace_back(
			End.x - Perp.x, End.y - Perp.y,
			End.x + Perp.x, End.y + Perp.y,
			Start.x - Perp.x, Start.y - Perp.y,
			Start.x + Perp.x, Start.y + Perp.y);
	}
	if(vQuads.empty())
		return;
	pGraphics->QuadsBegin();
	pGraphics->SetColor(Color);
	pGraphics->QuadsDrawFreeform(vQuads.data(), vQuads.size());
	pGraphics->QuadsEnd();
}
}

void CMalinkaTas::OnReset()
{
	ShutdownWorld();
	m_vFrames.clear();
	m_Metadata = {};
	m_aObservedInputs = {};
	m_aHaveInput = {};
	m_aWorldClientIds = {-1, -1};
	m_PlayCursor = 0;
	m_LastPlaybackTick = -1;
	m_SeekTarget = -1;
	m_ValidationCursor = 0;
	m_LastUpdateTime = time_get();
	m_LastAutoSave = m_LastUpdateTime;
	m_LastSeekTime = 0;
	m_Recording = false;
	m_Playing = false;
	m_Paused = false;
	m_HaveObservedInput = false;
	m_LatchedJump = false;
	m_WasFrozenInWorld = false;
	m_WasRewind = false;
	m_WasForward = false;
	m_AutoForwardActive = false;
	m_WaitingForReplayStart = false;
	m_ReleasePlaybackInput = false;
	m_Accumulator = 0.0;
	m_SmoothIntra = 0.0f;
	m_ObservedInput = {};
	m_ObservedMouse = vec2(1.0f, 0.0f);
	m_ObservedPos = vec2(0.0f, 0.0f);
	g_Config.m_MalinkaTasInit = 0;
	g_Config.m_MalinkaRecordReplay = 0;
	g_Config.m_MalinkaLoadReplay = 0;
	g_Config.m_MalinkaTasRewind = 0;
	g_Config.m_MalinkaTasForward = 0;
}

void CMalinkaTas::OnShutdown()
{
	ShutdownWorld();
}

void CMalinkaTas::OnStateChange(int NewState, int OldState)
{
	(void)OldState;
	if(NewState != IClient::STATE_ONLINE)
		OnReset();
}

bool CMalinkaTas::InteractionAllowed() const
{
	return Client()->State() == IClient::STATE_ONLINE &&
		static_cast<IEngineGraphics *>(Graphics())->WindowActive() &&
		!GameClient()->m_Menus.IsActive() && !GameClient()->m_Chat.IsActive() &&
		!GameClient()->m_GameConsole.IsActive() && !GameClient()->m_Snap.m_SpecInfo.m_Active;
}

float CMalinkaTas::GetStepRate() const
{
	return m_Playing ? 50.0f : static_cast<float>(std::clamp(g_Config.m_MalinkaTasTps, 1, 50));
}

void CMalinkaTas::SetCurrentMapMetadata()
{
	char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(GameClient()->Map()->Sha256(), aSha256, sizeof(aSha256));
	m_Metadata = {GameClient()->Map()->FullName(), aSha256, Malinka::Replay::PHYSICS, false};
}

bool CMalinkaTas::CurrentMapMatches() const
{
	if(m_Metadata.m_Legacy || !GameClient()->Map()->IsLoaded() || m_Metadata.m_Physics != Malinka::Replay::PHYSICS)
		return false;
	char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(GameClient()->Map()->Sha256(), aSha256, sizeof(aSha256));
	return m_Metadata.m_MapSha256 == aSha256;
}

void CMalinkaTas::EnsureWorldInitialized()
{
	if(m_WorldInitialized || Client()->State() != IClient::STATE_ONLINE)
		return;
	m_PrimarySlot = g_Config.m_ClDummy;
	m_aWorldClientIds = {GameClient()->m_aLocalIds[0], GameClient()->m_aLocalIds[1]};
	if(!GameClient()->m_PredictedWorld.GetCharacterById(m_aWorldClientIds[m_PrimarySlot]))
		return;
	m_pInitialWorld = std::make_unique<CGameWorld>();
	if(!m_pInitialWorld->CopyWorldIndependent(&GameClient()->m_PredictedWorld))
	{
		ShutdownWorld();
		PushTasWarning("TAS", "Cannot capture an independent map/physics snapshot (maximum 2000000 tiles).");
		return;
	}
	m_pInitialWorld->m_WorldConfig.m_PredictTeleports = g_Config.m_MalinkaPredictionTeleports != 0;
	m_pInitialWorld->m_WorldConfig.m_PredictEvents = true;
	m_pInitialWorld->m_PredictedEvents.clear();
	if(!g_Config.m_MalinkaTasPlayerPrediction)
	{
		for(int Id = 0; Id < MAX_CLIENTS; ++Id)
			if(Id != m_aWorldClientIds[0] && Id != m_aWorldClientIds[1])
				if(CCharacter *pChar = m_pInitialWorld->GetCharacterById(Id))
					delete pChar;
	}
	m_pWorld = std::make_unique<CGameWorld>();
	m_pWorldPredicted = std::make_unique<CGameWorld>();
	if(!m_pWorld->CopyWorldIndependent(m_pInitialWorld.get()) || !m_pWorldPredicted->CopyWorldIndependent(m_pWorld.get()))
	{
		ShutdownWorld();
		return;
	}
	m_WorldInitialized = true;
	g_Config.m_MalinkaTasInit = 1;
}

void CMalinkaTas::ShutdownWorld()
{
	m_pValidationWorld.reset();
	m_pWorldPredicted.reset();
	m_pWorld.reset();
	m_pInitialWorld.reset();
	m_SeekTarget = -1;
	m_WorldInitialized = false;
	g_Config.m_MalinkaTasInit = 0;
}

void CMalinkaTas::ResetWorldFromPredicted()
{
	ShutdownWorld();
	EnsureWorldInitialized();
}

void CMalinkaTas::StartRecording()
{
	if(GameClient()->m_FastPractice.Enabled() || GameClient()->m_MalinkaFent.IsRunning())
	{
		PushTasWarning("TAS", "Stop fast practice / Fent before starting an independent TAS world.");
		g_Config.m_MalinkaRecordReplay = 0;
		return;
	}
	StopPlayback();
	ResetWorldFromPredicted();
	if(!m_WorldInitialized)
	{
		PushTasWarning("TAS", "A live local character is required to initialize the TAS world.");
		g_Config.m_MalinkaRecordReplay = 0;
		return;
	}
	m_vFrames.clear();
	SetCurrentMapMetadata();
	m_PlayCursor = 0;
	m_Accumulator = 0.0;
	m_LastAutoSave = time_get();
	m_LastUpdateTime = m_LastAutoSave;
	m_LatchedJump = false;
	m_WasFrozenInWorld = false;
	m_Recording = true;
	m_ReleasePlaybackInput = false;
	g_Config.m_MalinkaRecordReplay = 1;
	PushTasWarning("TAS", "Local recording started. Server inputs are neutral. Enable /showall before recording entities.");
}

void CMalinkaTas::StopRecording()
{
	m_Recording = false;
	m_AutoForwardActive = false;
	m_SeekTarget = -1;
	m_Accumulator = 0.0;
	// Retain the independent initial snapshot for validation and a complete rewind.
}

bool CMalinkaTas::FindReplayPath(char *pBuf, int BufSize, bool ExactStemOnly) const
{
	pBuf[0] = '\0';
	const std::string Name = g_Config.m_MalinkaTasReplayName;
	if(!Malinka::Replay::SafeRelativeName(Name))
		return false;
	const auto Root = Malinka::Replay::Root(Storage());
	std::string Match;
	for(const auto &Relative : Malinka::Replay::List(Storage()))
	{
		const auto Path = std::filesystem::u8path(Relative);
		if(Relative == Name || Relative == Name + ".tas" ||
			((ExactStemOnly || Name.find('/') == std::string::npos) && Malinka::Replay::Utf8(Path.stem()) == Name))
		{
			if(!Match.empty())
				return false; // Ambiguous stems must be selected with their full relative path.
			Match = Relative;
		}
	}
	if(Match.empty())
		return false;
	const auto Path = Root / std::filesystem::u8path(Match);
	if(!Malinka::Replay::WithinRoot(Root, Path))
		return false;
	const auto Utf8 = Malinka::Replay::Utf8(Path);
	if(Utf8.size() >= static_cast<size_t>(BufSize))
		return false;
	str_copy(pBuf, Utf8.c_str(), BufSize);
	return true;
}

bool CMalinkaTas::GetReplaySavePath(char *pBuf, int BufSize, bool AutoSave) const
{
	pBuf[0] = '\0';
	std::string Name = g_Config.m_MalinkaTasReplayName[0] ? g_Config.m_MalinkaTasReplayName : "my_tas";
	if(!Malinka::Replay::SafeRelativeName(Name))
		return false;
	auto Relative = std::filesystem::u8path(Name);
	if(Relative.extension() == ".tas")
		Relative.replace_extension();
	if(AutoSave)
	{
		char aTimestamp[64];
		str_timestamp_format(aTimestamp, sizeof(aTimestamp), "%Y%m%d_%H%M%S");
		Relative = std::filesystem::path("auto") / Relative.parent_path() /
			(Relative.filename().string() + "_" + aTimestamp + ".tas");
	}
	else
		Relative.concat(".tas");
	const auto Root = Malinka::Replay::Root(Storage());
	const auto Path = Root / Relative;
	if(!Malinka::Replay::WithinRoot(Root, Path))
		return false;
	std::error_code Ec;
	std::filesystem::create_directories(Path.parent_path(), Ec);
	if(Ec)
		return false;
	const auto Utf8 = Malinka::Replay::Utf8(Path);
	if(Utf8.size() >= static_cast<size_t>(BufSize))
		return false;
	str_copy(pBuf, Utf8.c_str(), BufSize);
	return true;
}

bool CMalinkaTas::SaveReplayToPath(const char *pPath) const
{
	const auto Path = std::filesystem::u8path(pPath);
	const auto Root = Malinka::Replay::Root(Storage());
	if(!Malinka::Replay::WithinRoot(Root, Path) ||
		!Malinka::Replay::WithinRoot(Root, std::filesystem::path(Path).concat(".new")) ||
		!Malinka::Replay::WithinRoot(Root, std::filesystem::path(Path).concat(".bak")))
		return m_LastIoError = "Replay path escapes the save directory.", false;
	return Malinka::Replay::Write(Path, m_vFrames, m_Metadata, m_LastIoError);
}

bool CMalinkaTas::LoadReplayFromPath(const char *pPath)
{
	std::vector<SFrame> vFrames;
	Malinka::Replay::SMetadata Metadata;
	if(!Malinka::Replay::Read(std::filesystem::u8path(pPath), vFrames, Metadata, m_LastIoError))
		return false;
	ShutdownWorld();
	m_vFrames = std::move(vFrames);
	m_Metadata = std::move(Metadata);
	m_PrimarySlot = g_Config.m_ClDummy;
	m_PlayCursor = 0;
	return true;
}

bool CMalinkaTas::AtReplayStart() const
{
	if(m_vFrames.empty())
		return false;
	const CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_aLocalIds[m_PrimarySlot]);
	if(!pChar || distance(pChar->GetPos(), m_vFrames.front().m_Pos) > 1.0f)
		return false;
	if(m_vFrames.front().m_HasDummy)
	{
		const CCharacter *pDummy = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_aLocalIds[!m_PrimarySlot]);
		if(!pDummy || distance(pDummy->GetPos(), m_vFrames.front().m_DummyPos) > 1.0f)
			return false;
	}
	return true;
}

bool CMalinkaTas::StartPlayback()
{
	if(m_vFrames.empty())
	{
		char aPath[IO_MAX_PATH_LENGTH];
		if(!FindReplayPath(aPath, sizeof(aPath), false) || !LoadReplayFromPath(aPath))
		{
			PushTasWarning("TAS", m_LastIoError.empty() ? "Select a valid Malinka replay first." : m_LastIoError.c_str());
			g_Config.m_MalinkaLoadReplay = 0;
			return false;
		}
	}
	if(!CurrentMapMatches() || GameClient()->m_FastPractice.Enabled() || GameClient()->m_MalinkaFent.IsRunning())
	{
		PushTasWarning("TAS", "Playback blocked: map hash / physics mismatch, legacy format, or another private world is active.");
		g_Config.m_MalinkaLoadReplay = 0;
		return false;
	}
	m_PrimarySlot = g_Config.m_ClDummy;
	if(!AtReplayStart())
	{
		PushTasWarning("TAS", "Stand at the exact replay start (and dummy start) before playback.");
		g_Config.m_MalinkaLoadReplay = 0;
		return false;
	}
	StopRecording();
	g_Config.m_MalinkaRecordReplay = 0;
	m_Playing = true;
	m_PlayCursor = 0;
	m_LastPlaybackTick = -1;
	m_Accumulator = 0.0;
	m_ReleasePlaybackInput = false;
	m_WaitingForReplayStart = false;
	// Verify against the current world before any replay actions reach the server.
	m_pValidationWorld = std::make_unique<CGameWorld>();
	if(!m_pValidationWorld->CopyWorldIndependent(&GameClient()->m_PredictedWorld))
	{
		PushTasWarning("TAS", "Independent snapshot failed. TAS stopped without sending replay actions.");
		StopPlayback();
		StopRecording();
		g_Config.m_MalinkaRecordReplay = 0;
		return false;
	}
	m_pValidationWorld->m_WorldConfig.m_PredictTeleports = g_Config.m_MalinkaPredictionTeleports != 0;
	if(!g_Config.m_MalinkaTasPlayerPrediction)
		for(int Id = 0; Id < MAX_CLIENTS; ++Id)
			if(Id != GameClient()->m_aLocalIds[0] && Id != GameClient()->m_aLocalIds[1])
				if(auto *pChar = m_pValidationWorld->GetCharacterById(Id))
					delete pChar;
	m_ValidationCursor = 0;
	m_ValidationForPlayback = true;
	m_aWorldClientIds = {GameClient()->m_aLocalIds[0], GameClient()->m_aLocalIds[1]};
	PushTasWarning("TAS", "Checking replay physics in slices before playback; network input is neutral meanwhile.");
	return true;
}

void CMalinkaTas::StopPlayback()
{
	if(m_Playing)
		m_ReleasePlaybackInput = true;
	m_Playing = false;
	m_LastPlaybackTick = -1;
	m_WaitingForReplayStart = false;
	m_ValidationForPlayback = false;
	m_pValidationWorld.reset();
	g_Config.m_MalinkaLoadReplay = 0;
}

void CMalinkaTas::ClearReplay(bool KillPlayer)
{
	(void)KillPlayer;
	// Clear is local: it must not kill the real character during a private session.
	if(m_Recording && m_pInitialWorld)
	{
		m_vFrames.clear();
		if(!m_pWorld->CopyWorldIndependent(m_pInitialWorld.get()))
		{
			PushTasWarning("TAS", "Independent snapshot failed. TAS stopped without sending replay actions.");
			StopPlayback();
			StopRecording();
			g_Config.m_MalinkaRecordReplay = 0;
			return;
		}
		if(!m_pWorldPredicted->CopyWorldIndependent(m_pWorld.get()))
		{
			PushTasWarning("TAS", "Independent snapshot failed. TAS stopped without sending replay actions.");
			StopPlayback();
			StopRecording();
			g_Config.m_MalinkaRecordReplay = 0;
			return;
		}
		m_SeekTarget = -1;
		m_PlayCursor = 0;
		m_Accumulator = 0.0;
		return;
	}
	StopPlayback();
	StopRecording();
	m_vFrames.clear();
	ShutdownWorld();
}

void CMalinkaTas::DrainEvents(CGameWorld &World, bool EmitEvents)
{
	if(EmitEvents)
	{
		for(const auto &Event : World.m_PredictedEvents)
		{
			if(Event.m_EventId == NETEVENTTYPE_SOUNDWORLD && g_Config.m_MalinkaTasUseSound)
				GameClient()->m_Sounds.PlayAt(CSounds::CHN_WORLD, Event.m_ExtraInfo, 1.0f, Event.m_Pos);
			if(!g_Config.m_MalinkaTasShowEffects)
				continue;
			if(Event.m_EventId == NETEVENTTYPE_EXPLOSION)
				GameClient()->m_Effects.Explosion(Event.m_Pos, 1.0f);
			else if(Event.m_EventId == NETEVENTTYPE_HAMMERHIT)
				GameClient()->m_Effects.HammerHit(Event.m_Pos, 1.0f, g_Config.m_MalinkaTasUseSound ? 1.0f : 0.0f);
			else if(Event.m_EventId == NETEVENTTYPE_DAMAGEIND)
				GameClient()->m_Effects.DamageIndicator(Event.m_Pos, direction(Event.m_ExtraInfo / 256.0f), 1.0f);
		}
	}
	World.m_PredictedEvents.clear();
}

void CMalinkaTas::SimulationFailed()
{
	PushTasWarning("TAS", "Simulation unavailable (entity, tick limit or allocation failure). TAS stopped; no frame was accepted.");
	StopPlayback();
	StopRecording();
	m_pValidationWorld.reset();
	g_Config.m_MalinkaRecordReplay = 0;
}

bool CMalinkaTas::ApplyFrame(CGameWorld &World, const SFrame &Frame, bool EmitEvents)
{
	const int Primary = m_aWorldClientIds[m_PrimarySlot];
	const int Dummy = Frame.m_HasDummy ? m_aWorldClientIds[!m_PrimarySlot] : -1;
	if(Primary < 0 || (Frame.m_HasDummy && Dummy < 0) ||
		!Malinka::TickSimulation(World, {{Primary, Frame.m_Input}, {Dummy, Frame.m_DummyInput}}))
		return false;
	if(EmitEvents)
	{
		for(int Id : m_aWorldClientIds)
		{
			CCharacter *pChar = World.GetCharacterById(Id);
			if(!pChar)
				continue;
			const int Events = pChar->Core()->m_TriggeredEvents;
			if(g_Config.m_MalinkaTasUseSound && (Events & COREEVENT_GROUND_JUMP))
				GameClient()->m_Sounds.PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_JUMP, 1.0f, pChar->GetPos());
			if(Events & COREEVENT_AIR_JUMP)
			{
				if(g_Config.m_MalinkaTasShowEffects)
					GameClient()->m_Effects.AirJump(pChar->GetPos(), 1.0f, g_Config.m_MalinkaTasUseSound ? 1.0f : 0.0f);
				else if(g_Config.m_MalinkaTasUseSound)
					GameClient()->m_Sounds.PlayAt(CSounds::CHN_WORLD, SOUND_PLAYER_AIRJUMP, 1.0f, pChar->GetPos());
			}
		}
	}
	DrainEvents(World, EmitEvents);
	return true;
}

void CMalinkaTas::UpdatePredictedWorld(const CNetObj_PlayerInput &Input, vec2 Mouse)
{
	(void)Mouse;
	if(!m_WorldInitialized || !m_pWorld || !m_pWorldPredicted)
		return;
	if(!m_pWorldPredicted->CopyWorldIndependent(m_pWorld.get()))
	{
		PushTasWarning("TAS", "Independent snapshot failed. TAS stopped without sending replay actions.");
		StopPlayback();
		StopRecording();
		g_Config.m_MalinkaRecordReplay = 0;
		return;
	}
	SFrame Frame;
	Frame.m_Input = Input;
	Frame.m_HasDummy = m_pWorld->GetCharacterById(m_aWorldClientIds[!m_PrimarySlot]) != nullptr;
	Frame.m_DummyInput = m_aObservedInputs[!m_PrimarySlot];
	if(!ApplyFrame(*m_pWorldPredicted, Frame, false))
		SimulationFailed();
}

bool CMalinkaTas::IsLocalFrozenInWorld() const
{
	const CCharacter *pChar = m_pWorld ? m_pWorld->GetCharacterById(m_aWorldClientIds[m_PrimarySlot]) : nullptr;
	return pChar && (pChar->m_FreezeTime > 0 || pChar->Core()->m_DeepFrozen || pChar->Core()->m_LiveFrozen);
}

void CMalinkaTas::RewindRecording(int Amount)
{
	if(!m_Recording || m_vFrames.empty() || !m_pInitialWorld)
		return;
	const size_t Keep = m_vFrames.size() - std::min(m_vFrames.size(), static_cast<size_t>(std::max(1, Amount)));
	m_vFrames.resize(Keep);
	m_PlayCursor = static_cast<int>(Keep);
	if(!m_pWorld->CopyWorldIndependent(m_pInitialWorld.get()))
	{
		PushTasWarning("TAS", "Independent snapshot failed. TAS stopped without sending replay actions.");
		StopPlayback();
		StopRecording();
		g_Config.m_MalinkaRecordReplay = 0;
		return;
	}
	m_SeekCursor = 0;
	m_SeekTarget = static_cast<int>(Keep);
	m_Accumulator = 0.0;
	m_LatchedJump = false;
	m_AutoForwardActive = false;
}

void CMalinkaTas::RunSeekSlice()
{
	if(m_SeekTarget < 0 || !m_pWorld)
		return;
	const int64_t Deadline = time_get() + time_freq() / 250;
	for(int Budget = 0; Budget < 64 && m_SeekCursor < m_SeekTarget && time_get() < Deadline; ++Budget)
	{
		if(!ApplyFrame(*m_pWorld, m_vFrames[m_SeekCursor], false))
		{
			SimulationFailed();
			return;
		}
		++m_SeekCursor;
	}
	if(m_SeekCursor == m_SeekTarget)
	{
		m_SeekTarget = -1;
		if(!m_pWorldPredicted->CopyWorldIndependent(m_pWorld.get()))
		{
			PushTasWarning("TAS", "Independent snapshot failed. TAS stopped without sending replay actions.");
			StopPlayback();
			StopRecording();
			g_Config.m_MalinkaRecordReplay = 0;
			return;
		}
		m_WasFrozenInWorld = IsLocalFrozenInWorld();
	}
}

void CMalinkaTas::StepForwardRecording()
{
	if(!m_Recording || !m_HaveObservedInput || m_SeekTarget >= 0 || !m_pWorld)
		return;
	if(m_vFrames.size() >= Malinka::Replay::MAX_FRAMES)
	{
		g_Config.m_MalinkaRecordReplay = 0;
		StopRecording();
		PushTasWarning("TAS", "Recording stopped at the 180000-frame resource limit. Save the buffer.");
		return;
	}
	CCharacter *pChar = m_pWorld->GetCharacterById(m_aWorldClientIds[m_PrimarySlot]);
	if(!pChar)
	{
		g_Config.m_MalinkaTasPause = 1;
		return;
	}
	SFrame Frame;
	Frame.m_Tick = static_cast<int>(m_vFrames.size());
	Frame.m_Input = m_aObservedInputs[m_PrimarySlot];
	if(m_LatchedJump && m_PrimarySlot == g_Config.m_ClDummy)
		Frame.m_Input.m_Jump = 1;
	Frame.m_Mouse = vec2(Frame.m_Input.m_TargetX, Frame.m_Input.m_TargetY);
	Frame.m_Pos = pChar->GetPos();
	if(CCharacter *pDummy = m_pWorld->GetCharacterById(m_aWorldClientIds[!m_PrimarySlot]))
	{
		Frame.m_HasDummy = true;
		Frame.m_DummyInput = m_aObservedInputs[!m_PrimarySlot];
		Frame.m_DummyPos = pDummy->GetPos();
		if(g_Config.m_ClDummyHammer)
		{
			const int PreviousFire = m_vFrames.empty() ? Frame.m_DummyInput.m_Fire : m_vFrames.back().m_DummyInput.m_Fire;
			Frame.m_DummyInput.m_Fire = Frame.m_Tick % 25 == 0 ? ((PreviousFire + 1) | 1) & INPUT_STATE_MASK : PreviousFire;
			Frame.m_DummyInput.m_WantedWeapon = WEAPON_HAMMER + 1;
			const vec2 Aim = Malinka::FiniteAim(Frame.m_Pos - Frame.m_DummyPos);
			Frame.m_DummyInput.m_TargetX = static_cast<int>(Aim.x);
			Frame.m_DummyInput.m_TargetY = static_cast<int>(Aim.y);
		}
	}
	if(!Malinka::Replay::ValidInput(Frame.m_Input) || (Frame.m_HasDummy && !Malinka::Replay::ValidInput(Frame.m_DummyInput)))
	{
		g_Config.m_MalinkaTasPause = 1;
		PushTasWarning("TAS", "Invalid local input; recording paused instead of storing corrupt data.");
		return;
	}
	if(!ApplyFrame(*m_pWorld, Frame, true))
	{
		SimulationFailed();
		return;
	}
	m_vFrames.push_back(Frame);
	m_PlayCursor = static_cast<int>(m_vFrames.size());
	m_LatchedJump = false;
	const bool Frozen = IsLocalFrozenInWorld();
	if(Frozen && !m_WasFrozenInWorld)
	{
		if(g_Config.m_MalinkaTasAutoRewind)
			RewindRecording(GetTasStepAmount());
		else if(g_Config.m_MalinkaTasAutoForward)
			m_AutoForwardActive = true;
		if(g_Config.m_MalinkaTickControlPause)
			g_Config.m_MalinkaTasPause = 1;
	}
	m_WasFrozenInWorld = Frozen;
}

void CMalinkaTas::AutoForwardRecording()
{
	// At most 32 ticks / 4ms per update; a permanent freeze cannot hang the UI.
	const int64_t Deadline = time_get() + time_freq() / 250;
	for(int Budget = 0; Budget < 32 && m_AutoForwardActive && m_Recording && IsLocalFrozenInWorld() && time_get() < Deadline; ++Budget)
		StepForwardRecording();
	if(!IsLocalFrozenInWorld())
	{
		m_AutoForwardActive = false;
		if(g_Config.m_MalinkaTickControlPause)
			g_Config.m_MalinkaTasPause = 1;
	}
}

void CMalinkaTas::RunValidationSlice()
{
	if(!m_pValidationWorld)
		return;
	const int64_t Deadline = time_get() + time_freq() / 250;
	for(int Budget = 0; Budget < 64 && m_ValidationCursor < static_cast<int>(m_vFrames.size()) && time_get() < Deadline; ++Budget)
	{
		const SFrame &Frame = m_vFrames[m_ValidationCursor];
		const CCharacter *pChar = m_pValidationWorld->GetCharacterById(m_aWorldClientIds[m_PrimarySlot]);
		const CCharacter *pDummy = m_pValidationWorld->GetCharacterById(m_aWorldClientIds[!m_PrimarySlot]);
		if(!pChar || distance(pChar->GetPos(), Frame.m_Pos) > 0.5f ||
			(Frame.m_HasDummy && (!pDummy || distance(pDummy->GetPos(), Frame.m_DummyPos) > 0.5f)))
		{
			char aMessage[256];
			str_format(aMessage, sizeof(aMessage), "Physics diverged at frame %d. Current world state differs from the recording; playback blocked.", m_ValidationCursor);
			PushTasWarning("TAS validation", aMessage);
			m_pValidationWorld.reset();
			if(m_ValidationForPlayback)
				StopPlayback();
			return;
		}
		if(!ApplyFrame(*m_pValidationWorld, Frame, false))
		{
			SimulationFailed();
			return;
		}
		++m_ValidationCursor;
	}
	if(m_ValidationCursor == static_cast<int>(m_vFrames.size()))
	{
		m_pValidationWorld.reset();
		m_ValidationForPlayback = false;
		PushTasWarning("TAS validation", "All recorded frame positions match this simulation. This is not a server-side guarantee.");
	}
}

void CMalinkaTas::ObserveSlotInput(int Slot, const CNetObj_PlayerInput &Input)
{
	if(Slot < 0 || Slot >= NUM_DUMMIES)
		return;
	m_aObservedInputs[Slot] = Input;
	m_aHaveInput[Slot] = true;
}

void CMalinkaTas::ObserveInput(const CNetObj_PlayerInput &Input, vec2 Pos)
{
	ObserveSlotInput(g_Config.m_ClDummy, Input);
	m_HaveObservedInput = true;
	m_ObservedInput = Input;
	m_LatchedJump = m_LatchedJump || Input.m_Jump != 0;
	m_ObservedMouse = Malinka::FiniteAim(vec2(Input.m_TargetX, Input.m_TargetY));
	m_ObservedPos = Pos;
}

bool CMalinkaTas::NeedsNetworkInput() const
{
	return m_Recording || m_Playing || g_Config.m_MalinkaRecordReplay || m_ReleasePlaybackInput;
}

int CMalinkaTas::PrepareNetworkInput(CNetObj_PlayerInput &Input, int Slot, int PredTick)
{
	if(m_Recording || g_Config.m_MalinkaRecordReplay)
		return 1;
	if(m_ReleasePlaybackInput && !m_Playing)
	{
		if(Slot == !g_Config.m_ClDummy || !Client()->DummyConnected())
			m_ReleasePlaybackInput = false;
		return 3;
	}
	if(!m_Playing)
		return 0;
	if(!CurrentMapMatches() || (!m_WaitingForReplayStart && !GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_aLocalIds[m_PrimarySlot])))
	{
		StopPlayback();
		return 3;
	}
	if(m_pValidationWorld)
		return 3;
	if(!InteractionAllowed() || g_Config.m_MalinkaTasPause)
	{
		PushTasWarning("TAS", "Real playback cannot pause the server. Playback stopped on pause/menu/chat/focus loss.");
		StopPlayback();
		return 3;
	}
	if(m_WaitingForReplayStart)
	{
		m_LastPlaybackTick = -1;
		if(!AtReplayStart())
			return 3;
		m_WaitingForReplayStart = false;
		m_PlayCursor = 0;
	}
	if(m_LastPlaybackTick < 0 && !AtReplayStart())
	{
		PushTasWarning("TAS", "Start position changed while validating. Playback stopped.");
		StopPlayback();
		return 3;
	}
	if(m_LastPlaybackTick >= 0 && PredTick != m_LastPlaybackTick)
	{
		if(PredTick != m_LastPlaybackTick + 1)
		{
			PushTasWarning("TAS", "Prediction skipped/reversed ticks. Playback stopped rather than skipping replay inputs.");
			StopPlayback();
			return 3;
		}
		++m_PlayCursor;
	}
	m_LastPlaybackTick = PredTick;
	const CCharacter *pRealChar = GameClient()->m_PredictedWorld.GetCharacterById(GameClient()->m_aLocalIds[m_PrimarySlot]);
	const bool Frozen = pRealChar && (pRealChar->m_FreezeTime > 0 || pRealChar->Core()->m_DeepFrozen);
	if(m_PlayCursor >= static_cast<int>(m_vFrames.size()) || (g_Config.m_MalinkaTasAutoReplayFreeze && Frozen))
	{
		if(g_Config.m_MalinkaTasAutoReplay)
		{
			m_WaitingForReplayStart = true;
			m_LastPlaybackTick = -1;
			m_PlayCursor = 0;
			if(g_Config.m_MalinkaTasAutoReplayKill)
				GameClient()->m_Chat.SendChat(0, "/kill");
		}
		else
			StopPlayback();
		return 3;
	}
	CNetObj_PlayerInput ReplayInput;
	if(!GetPlaybackInput(ReplayInput, Slot))
		return 3;
	const int Flags = Input.m_PlayerFlags;
	Input = ReplayInput;
	Input.m_PlayerFlags = Flags;
	return 2;
}

bool CMalinkaTas::GetPlaybackInput(CNetObj_PlayerInput &Out, int Slot) const
{
	if(!m_Playing || m_vFrames.empty() || m_pValidationWorld || m_WaitingForReplayStart)
		return false;
	if(Slot < 0)
		Slot = g_Config.m_ClDummy;
	const SFrame &Frame = m_vFrames[std::clamp(m_PlayCursor, 0, static_cast<int>(m_vFrames.size()) - 1)];
	if(Slot == m_PrimarySlot)
		Out = Frame.m_Input;
	else if(Frame.m_HasDummy)
		Out = Frame.m_DummyInput;
	else
		return false;
	return true;
}

bool CMalinkaTas::HasRenderMouseOverride() const
{
	return m_Recording || (m_Playing && g_Config.m_MalinkaTasShowAim);
}

vec2 CMalinkaTas::GetRenderMousePos() const
{
	CNetObj_PlayerInput Input;
	if(GetPlaybackInput(Input))
		return Malinka::FiniteAim(vec2(Input.m_TargetX, Input.m_TargetY));
	return Malinka::FiniteAim(GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy]);
}

bool CMalinkaTas::IsWorldActive() const
{
	return m_Recording && m_WorldInitialized;
}

vec2 CMalinkaTas::GetInterpolatedPlayerPos() const
{
	if(!IsWorldActive() || !m_pWorld)
		return GameClient()->m_LocalCharacterPos;
	const int Id = m_aWorldClientIds[g_Config.m_ClDummy];
	CCharacter *pChar = m_pWorld->GetCharacterById(Id);
	CCharacter *pNext = m_pWorldPredicted ? m_pWorldPredicted->GetCharacterById(Id) : nullptr;
	if(!pChar)
		return GameClient()->m_LocalCharacterPos;
	return mix(pChar->GetPos(), pNext ? pNext->GetPos() : pChar->GetPos(), m_SmoothIntra);
}

void CMalinkaTas::PushTasWarning(const char *pTitle, const char *pMessage) const
{
	GameClient()->m_MalinkaVisuals.PushNotification(SWarning(pTitle, pMessage));
	Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "malinka/tas", pMessage);
}

void CMalinkaTas::SaveReplay()
{
	char aPath[IO_MAX_PATH_LENGTH];
	if(m_vFrames.empty() || !GetReplaySavePath(aPath, sizeof(aPath), false) || !SaveReplayToPath(aPath))
	{
		PushTasWarning("TAS", m_LastIoError.empty() ? "Cannot save: empty buffer or invalid save name/path." : m_LastIoError.c_str());
		return;
	}
	PushTasWarning("TAS", "Replay saved with checksum, map identity, and backup of any replaced file.");
}

void CMalinkaTas::LoadSelectedReplay()
{
	if(m_Recording || m_Playing)
	{
		PushTasWarning("TAS", "Stop recording/playback before replacing the current buffer.");
		return;
	}
	char aPath[IO_MAX_PATH_LENGTH];
	if(!FindReplayPath(aPath, sizeof(aPath), false) || !LoadReplayFromPath(aPath))
	{
		PushTasWarning("TAS", m_LastIoError.empty() ? "No unambiguous replay selected." : m_LastIoError.c_str());
		return;
	}
	PushTasWarning("TAS", m_Metadata.m_Legacy ? "Legacy Pasta path loaded for viewing only; map identity/physics are unknown, playback is blocked." : "Replay loaded; playback requires matching map/physics and start state.");
}

void CMalinkaTas::ValidateReplay()
{
	if(m_Recording || m_Playing || m_vFrames.empty())
	{
		PushTasWarning("TAS validation", "Stop recording/playback and load a replay before validating.");
		return;
	}
	std::string Error;
	if(!Malinka::Replay::ValidateFrames(m_vFrames, Error) || !CurrentMapMatches())
	{
		PushTasWarning("TAS validation", Error.empty() ? "Map identity/physics do not match (or legacy format)." : Error.c_str());
		return;
	}
	m_pValidationWorld = std::make_unique<CGameWorld>();
	if(m_pInitialWorld)
		if(!m_pValidationWorld->CopyWorldIndependent(m_pInitialWorld.get()))
		{
			PushTasWarning("TAS", "Independent snapshot failed. TAS stopped without sending replay actions.");
			StopPlayback();
			StopRecording();
			g_Config.m_MalinkaRecordReplay = 0;
			return;
		}
	else
	{
		m_PrimarySlot = g_Config.m_ClDummy;
		if(!AtReplayStart())
		{
			m_pValidationWorld.reset();
			PushTasWarning("TAS validation", "Stand at both replay start positions so the current world can be simulated.");
			return;
		}
		m_aWorldClientIds = {GameClient()->m_aLocalIds[0], GameClient()->m_aLocalIds[1]};
		if(!m_pValidationWorld->CopyWorldIndependent(&GameClient()->m_PredictedWorld))
		{
			PushTasWarning("TAS", "Independent snapshot failed. TAS stopped without sending replay actions.");
			StopPlayback();
			StopRecording();
			g_Config.m_MalinkaRecordReplay = 0;
			return;
		}
	}
	m_ValidationCursor = 0;
	m_ValidationForPlayback = false;
}

void CMalinkaTas::ReportReplayTime()
{
	char aMessage[128];
	str_format(aMessage, sizeof(aMessage), "%d input frames, %.2fs game time at 50 ticks/s (not an independently verified finish time).", static_cast<int>(m_vFrames.size()), m_vFrames.size() / 50.0);
	PushTasWarning("TAS", aMessage);
}

void CMalinkaTas::SyncNow()
{
	PushTasWarning("Replay vault", "Vault sync is not implemented. Two official zlib/JSON samples were measured, but a validated importer is not connected.");
}

void CMalinkaTas::OnUpdate()
{
	const int64_t Now = time_get();
	const double Delta = m_LastUpdateTime ? std::clamp(static_cast<double>(Now - m_LastUpdateTime) / time_freq(), 0.0, 0.1) : 0.0;
	m_LastUpdateTime = Now;
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	if(g_Config.m_MalinkaTasRespawn)
	{
		ClearReplay(false);
		g_Config.m_MalinkaTasRespawn = 0;
	}
	if(g_Config.m_MalinkaRecordReplay && !m_Recording)
		StartRecording();
	else if(!g_Config.m_MalinkaRecordReplay && m_Recording)
		StopRecording();
	if(g_Config.m_MalinkaLoadReplay && !m_Playing)
		StartPlayback();
	else if(!g_Config.m_MalinkaLoadReplay && m_Playing)
		StopPlayback();
	if((m_Recording || m_Playing) && !CurrentMapMatches())
	{
		StopPlayback();
		StopRecording();
		g_Config.m_MalinkaRecordReplay = 0;
		PushTasWarning("TAS", "Map/physics changed. TAS stopped.");
	}
	if(m_Recording && (m_aWorldClientIds[0] != GameClient()->m_aLocalIds[0] || m_aWorldClientIds[1] != GameClient()->m_aLocalIds[1]))
	{
		StopRecording();
		g_Config.m_MalinkaRecordReplay = 0;
		PushTasWarning("TAS", "A participant disconnected or changed identity. Save the current buffer and start a new recording.");
	}
	RunValidationSlice();
	RunSeekSlice();
	m_Paused = g_Config.m_MalinkaTasPause || !InteractionAllowed() || m_SeekTarget >= 0;
	const bool Rewind = g_Config.m_MalinkaTasRewind != 0;
	const bool Forward = g_Config.m_MalinkaTasForward != 0;
	if(m_Recording && InteractionAllowed() && m_SeekTarget < 0)
	{
		const bool SeekReady = Now >= m_LastSeekTime + time_freq() / 20;
		const bool DoRewind = Rewind && (!m_WasRewind || (!g_Config.m_MalinkaTickControlStep && SeekReady));
		const bool DoForward = Forward && (!m_WasForward || (!g_Config.m_MalinkaTickControlStep && SeekReady));
		if(DoRewind)
			RewindRecording(GetTasStepAmount());
		else if(DoForward)
			for(int Count = 0; Count < GetTasStepAmount(); ++Count)
				StepForwardRecording();
		if(DoRewind || DoForward)
		{
			m_LastSeekTime = Now;
			if(g_Config.m_MalinkaTickControlPause)
				g_Config.m_MalinkaTasPause = 1;
		}
		if(!Rewind && !Forward && !m_Paused)
		{
			m_Accumulator = std::min(5.0, m_Accumulator + Delta * GetStepRate());
			for(int Budget = 0; Budget < 5 && m_Accumulator >= 1.0 && m_Recording && m_SeekTarget < 0; ++Budget)
			{
				m_Accumulator -= 1.0;
				StepForwardRecording();
				if(g_Config.m_MalinkaTasPause)
					break;
			}
		}
		else
			m_Accumulator = 0.0;
		if(m_AutoForwardActive)
			AutoForwardRecording();
		m_SmoothIntra = m_Paused || m_SeekTarget >= 0 ? 0.0f : std::clamp(static_cast<float>(m_Accumulator), 0.0f, 1.0f);
		if(m_HaveObservedInput && m_SeekTarget < 0)
			UpdatePredictedWorld(m_aObservedInputs[m_PrimarySlot], m_ObservedMouse);
	}
	m_WasRewind = Rewind;
	m_WasForward = Forward;
	if(m_Recording && g_Config.m_MalinkaAutoSaveReplay && !m_vFrames.empty() &&
		Now > m_LastAutoSave + static_cast<int64_t>(std::max(1, g_Config.m_MalinkaAutoSaveReplayInterval)) * time_freq())
	{
		char aPath[IO_MAX_PATH_LENGTH];
		if(!GetReplaySavePath(aPath, sizeof(aPath), true) || !SaveReplayToPath(aPath))
			PushTasWarning("TAS autosave", m_LastIoError.empty() ? "Autosave failed." : m_LastIoError.c_str());
		m_LastAutoSave = Now;
	}
}

void CMalinkaTas::RenderStartEndPos()
{
	if(!g_Config.m_MalinkaTasDrawStartEndPos || m_vFrames.empty())
		return;

	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(LocalId < 0)
		return;

	CTeeRenderInfo TeeInfo = GameClient()->m_aClients[LocalId].m_RenderInfo;
	TeeInfo.m_Size = 64.0f;
	const CAnimState *pIdleState = CAnimState::GetIdle();

	CTeeRenderInfo StartInfo = TeeInfo;
	StartInfo.m_CustomColoredSkin = true;
	StartInfo.m_ColorBody = ColorRGBA(0.15f, 1.0f, 0.35f, 1.0f);
	StartInfo.m_ColorFeet = ColorRGBA(0.15f, 1.0f, 0.35f, 1.0f);
	GameClient()->RenderTools()->RenderTee(pIdleState, &StartInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), m_vFrames.front().m_Pos);

	if(!m_Recording && m_vFrames.size() > 1)
	{
		CTeeRenderInfo EndInfo = TeeInfo;
		EndInfo.m_CustomColoredSkin = true;
		EndInfo.m_ColorBody = ColorRGBA(1.0f, 0.2f, 0.95f, 1.0f);
		EndInfo.m_ColorFeet = ColorRGBA(1.0f, 0.2f, 0.95f, 1.0f);
		GameClient()->RenderTools()->RenderTee(pIdleState, &EndInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), m_vFrames.back().m_Pos);
	}
}

void CMalinkaTas::RenderReplayPath()
{
	if(!g_Config.m_MalinkaTasDrawPath || m_vFrames.size() < 2 || !m_Playing || m_Recording)
		return;

	const ColorRGBA PathColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_MalinkaTasDrawPathColor));
	int StartIndex = 0;
	int EndIndex = (int)m_vFrames.size() - 1;
	if(g_Config.m_MalinkaTasDrawPathSegmented)
	{
		const int Pivot = std::clamp(m_PlayCursor, 0, EndIndex);
		StartIndex = Pivot;
		EndIndex = std::min((int)m_vFrames.size() - 1, Pivot + 180);
	}

	Graphics()->TextureClear();
	if(g_Config.m_MalinkaTasDrawPathMode == 0)
	{
		std::vector<vec2> vPoints;
		vPoints.reserve(EndIndex - StartIndex + 1);
		for(int i = StartIndex; i <= EndIndex; ++i)
			if(i % 2 == 0)
				vPoints.push_back(m_vFrames[i].m_Pos);
		const auto DotTexture = GameClient()->m_ParticlesSkin.m_aSpriteParticles[SPRITE_PART_BALL - SPRITE_PART_SLICE];
		RenderTasPoints(Graphics(), DotTexture, vPoints, PathColor, 4.0f);
	}
	else
	{
		std::vector<IGraphics::CLineItem> vSegments;
		vSegments.reserve(EndIndex - StartIndex);
		for(int i = StartIndex; i < EndIndex; ++i)
		{
			const vec2 Pos1 = m_vFrames[i].m_Pos;
			const vec2 Pos2 = m_vFrames[i + 1].m_Pos;
			vSegments.emplace_back(Pos1.x, Pos1.y, Pos2.x, Pos2.y);
		}
		RenderTasLines(Graphics(), vSegments, PathColor, 1.35f, GameClient()->m_Camera.m_Zoom);
	}
}

void CMalinkaTas::RenderPredictionPath()
{
	if(!g_Config.m_MalinkaTasDrawPredictionPath || !m_WorldInitialized || m_pWorld == nullptr || m_Playing)
		return;

	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(LocalId < 0)
		return;
	CCharacter *pChar = m_pWorld->GetCharacterById(LocalId);
	if(pChar == nullptr)
		return;

	CGameWorld TempWorld;
	TempWorld.Init(Collision(), GameClient()->m_GameWorld.TuningList(), nullptr);
	if(!TempWorld.CopyWorldIndependent(m_pWorld.get()))
	{
		PushTasWarning("TAS", "Independent snapshot failed. TAS stopped without sending replay actions.");
		StopPlayback();
		StopRecording();
		g_Config.m_MalinkaRecordReplay = 0;
		return;
	}
	CCharacter *pTempChar = TempWorld.GetCharacterById(LocalId);
	if(pTempChar == nullptr)
		return;

	const CNetObj_PlayerInput PredictInput = m_Playing && !m_vFrames.empty() ? m_vFrames[std::clamp(m_PlayCursor, 0, (int)m_vFrames.size() - 1)].m_Input : m_ObservedInput;
	std::vector<vec2> vPath;
	const int Ticks = 30;
	vPath.reserve(Ticks + 1);
	for(int t = 0; t < Ticks; ++t)
	{
		vPath.push_back(pTempChar->GetPos());
		SFrame Frame;
		Frame.m_Input = PredictInput;
		Frame.m_HasDummy = TempWorld.GetCharacterById(m_aWorldClientIds[!m_PrimarySlot]) != nullptr;
		Frame.m_DummyInput = m_aObservedInputs[!m_PrimarySlot];
		if(!ApplyFrame(TempWorld, Frame, false))
			return;
		pTempChar = TempWorld.GetCharacterById(LocalId);
		if(!pTempChar)
			break;
	}
	if(pTempChar)
		vPath.push_back(pTempChar->GetPos());

	const ColorRGBA LocalColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_MalinkaTasDrawPredictionPathColor));
	const ColorRGBA FrozenColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_MalinkaTasDrawPredictionPathColorFrozen));
	Graphics()->TextureClear();
	if(g_Config.m_MalinkaTasDrawPredictionPathMode == 0)
	{
		std::vector<vec2> vLocalDots;
		std::vector<vec2> vFrozenDots;
		for(const vec2 &Pos : vPath)
		{
			bool Frozen = false;
			if(Collision() != nullptr)
			{
				const int Index = Collision()->GetPureMapIndex(Pos);
				const int Tile = Collision()->GetTileIndex(Index);
				const int FrontTile = Collision()->GetFrontTileIndex(Index);
				Frozen = Tile == TILE_FREEZE || FrontTile == TILE_FREEZE || Tile == TILE_DFREEZE || FrontTile == TILE_DFREEZE || Tile == TILE_LFREEZE || FrontTile == TILE_LFREEZE;
			}
			(Frozen ? vFrozenDots : vLocalDots).push_back(Pos);
		}
		const auto DotTexture = GameClient()->m_ParticlesSkin.m_aSpriteParticles[SPRITE_PART_BALL - SPRITE_PART_SLICE];
		RenderTasPoints(Graphics(), DotTexture, vLocalDots, LocalColor, 3.5f);
		RenderTasPoints(Graphics(), DotTexture, vFrozenDots, FrozenColor, 3.5f);
	}
	else
	{
		std::vector<IGraphics::CLineItem> vLocalSegments;
		std::vector<IGraphics::CLineItem> vFrozenSegments;
		for(size_t i = 1; i < vPath.size(); ++i)
		{
			bool Frozen = false;
			if(Collision() != nullptr)
			{
				const int Index = Collision()->GetPureMapIndex(vPath[i - 1]);
				const int Tile = Collision()->GetTileIndex(Index);
				const int FrontTile = Collision()->GetFrontTileIndex(Index);
				Frozen = Tile == TILE_FREEZE || FrontTile == TILE_FREEZE || Tile == TILE_DFREEZE || FrontTile == TILE_DFREEZE || Tile == TILE_LFREEZE || FrontTile == TILE_LFREEZE;
			}
			(Frozen ? vFrozenSegments : vLocalSegments).emplace_back(vPath[i - 1].x, vPath[i - 1].y, vPath[i].x, vPath[i].y);
		}
		RenderTasLines(Graphics(), vLocalSegments, LocalColor, 1.2f, GameClient()->m_Camera.m_Zoom);
		RenderTasLines(Graphics(), vFrozenSegments, FrozenColor, 1.2f, GameClient()->m_Camera.m_Zoom);
	}
}

void CMalinkaTas::RenderTasWorld()
{
	if(!m_WorldInitialized || m_pWorld == nullptr || !m_Recording)
		return;

	const float Intra = std::clamp(m_SmoothIntra, 0.0f, 1.0f);
	const int TasClientId = CPlayers::CLIENT_ID_TAS;
	for(int DummyIdx = 0; DummyIdx < NUM_DUMMIES; ++DummyIdx)
	{
		const int LocalId = m_aWorldClientIds[DummyIdx];
		if(LocalId < 0)
			continue;

		CCharacter *pCommitted = m_pWorld->GetCharacterById(LocalId);
		CCharacter *pPredicted = m_pWorldPredicted != nullptr ? m_pWorldPredicted->GetCharacterById(LocalId) : nullptr;
		if(pCommitted == nullptr)
			continue;

		const CCharacterCore &PrevCore = *pCommitted->Core();
		const CCharacterCore &CurCore = pPredicted != nullptr ? *pPredicted->Core() : PrevCore;
		const vec2 AimDir = normalize(length(GetRenderMousePos()) > 0.001f ? GetRenderMousePos() : vec2(1.0f, 0.0f));

		CNetObj_Character PrevObj{};
		CNetObj_Character CurObj{};
		PrevObj.m_X = (int)pCommitted->GetPos().x;
		PrevObj.m_Y = (int)pCommitted->GetPos().y;
		PrevObj.m_VelX = (int)(PrevCore.m_Vel.x * 256.0f);
		PrevObj.m_VelY = (int)(PrevCore.m_Vel.y * 256.0f);
		PrevObj.m_Angle = (int)(angle(AimDir) * 256.0f);
		PrevObj.m_Direction = PrevCore.m_Direction;
		PrevObj.m_Jumped = PrevCore.m_Jumped;
		PrevObj.m_HookState = PrevCore.m_HookState;
		PrevObj.m_HookX = (int)PrevCore.m_HookPos.x;
		PrevObj.m_HookY = (int)PrevCore.m_HookPos.y;
		PrevObj.m_HookedPlayer = PrevCore.HookedPlayer();
		PrevObj.m_AttackTick = pCommitted->GetAttackTick();
		PrevObj.m_Weapon = PrevCore.m_ActiveWeapon;

		CurObj = PrevObj;
		if(pPredicted != nullptr)
		{
			CurObj.m_X = (int)pPredicted->GetPos().x;
			CurObj.m_Y = (int)pPredicted->GetPos().y;
			CurObj.m_VelX = (int)(CurCore.m_Vel.x * 256.0f);
			CurObj.m_VelY = (int)(CurCore.m_Vel.y * 256.0f);
			CurObj.m_Direction = CurCore.m_Direction;
			CurObj.m_Jumped = CurCore.m_Jumped;
			CurObj.m_HookState = CurCore.m_HookState;
			CurObj.m_HookX = (int)CurCore.m_HookPos.x;
			CurObj.m_HookY = (int)CurCore.m_HookPos.y;
			CurObj.m_HookedPlayer = CurCore.HookedPlayer();
			CurObj.m_Weapon = CurCore.m_ActiveWeapon;
		}

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[LocalId].m_RenderInfo;
		TeeInfo.m_Size = 64.0f;
		TeeInfo.m_TeeRenderFlags = 0;
		if(CurCore.m_FreezeEnd != 0 || CurCore.m_DeepFrozen)
			TeeInfo.m_TeeRenderFlags |= TEE_EFFECT_FROZEN | TEE_NO_WEAPON;
		else if(CurCore.m_LiveFrozen)
			TeeInfo.m_TeeRenderFlags |= TEE_EFFECT_FROZEN;
		TeeInfo.m_ColorBody.a = 0.9f;
		TeeInfo.m_ColorFeet.a = 0.9f;

		GameClient()->m_Players.RenderHook(Graphics()->GetScreen(), &PrevObj, &CurObj, &TeeInfo, TasClientId, Intra);
		GameClient()->m_Players.RenderPlayer(Graphics()->GetScreen(), &PrevObj, &CurObj, &TeeInfo, TasClientId, Intra);
	}
}

void CMalinkaTas::RenderWorldCharacters()
{
	RenderTasWorld();
}

void CMalinkaTas::RenderHud()
{
	if(!g_Config.m_MalinkaTasHud || GameClient()->m_Menus.IsActive())
		return;
	if(!m_Recording && !m_Playing)
		return;

	const float Width = 300.0f * Graphics()->ScreenAspect();
	const float Height = 300.0f;
	Graphics()->MapScreen(CScreenRect(0.0f, 0.0f, Width, Height));

	const float X = Width - 95.0f;
	const float Y = 94.0f;
	const float BoxW = 88.0f;
	const float BoxH = 64.0f;
	CUIRect HudRect = {X, Y, BoxW, BoxH};
	HudRect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.34f), IGraphics::CORNER_ALL, 3.0f);

	const int TickValue = m_Playing ? std::max(0, m_PlayCursor) : (int)m_vFrames.size();
	const float Seconds = TickValue / 50.0f;
	char aLine1[64];
	str_format(aLine1, sizeof(aLine1), "%s  %.2fs", m_Recording ? "REC" : "PLAY", Seconds);

	const CNetObj_PlayerInput *pHudInput = nullptr;
	CNetObj_PlayerInput PlaybackInput;
	if(m_Playing && GetPlaybackInput(PlaybackInput))
		pHudInput = &PlaybackInput;
	else if(m_Recording && m_HaveObservedInput)
		pHudInput = &m_ObservedInput;

	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	CCharacter *pTasChar = (m_WorldInitialized && m_pWorld != nullptr && LocalId >= 0) ? m_pWorld->GetCharacterById(LocalId) : nullptr;
	int Cooldown = 0;
	const char *pWeaponName = "HAM";
	if(pTasChar != nullptr)
	{
		static const char *s_apWeaponNames[] = {"HAM", "GUN", "SG", "GR", "LAS", "NIN"};
		const int Weapon = std::clamp(pTasChar->GetActiveWeapon(), 0, NUM_WEAPONS - 1);
		pWeaponName = s_apWeaponNames[Weapon];
		const int Zone = pTasChar->GetOverriddenTuneZone();
		const CTuningParams *pTuning = &GameClient()->m_GameWorld.TuningList()[std::clamp(Zone, 0, TuneZone::NUM - 1)];
		if(pTuning != nullptr)
		{
			const int FireDelayTicks = std::max(0, round_to_int(pTuning->GetWeaponFireDelay(Weapon) * Client()->GameTickSpeed()));
			Cooldown = std::max(0, pTasChar->GetAttackTick() + FireDelayTicks - (m_pWorld != nullptr ? m_pWorld->m_GameTick : Client()->GameTick(g_Config.m_ClDummy)));
		}
	}

	char aLine2[64];
	str_format(aLine2, sizeof(aLine2), "%s CD:%d", pWeaponName, Cooldown);

	char aLine3[96];
	str_format(
		aLine3,
		sizeof(aLine3),
		"D:%d J:%d H:%d F:%d",
		pHudInput != nullptr ? pHudInput->m_Direction : 0,
		pHudInput != nullptr && pHudInput->m_Jump ? 1 : 0,
		pHudInput != nullptr && pHudInput->m_Hook ? 1 : 0,
		pHudInput != nullptr && pHudInput->m_Fire ? 1 : 0);

	TextRender()->TextColor(m_Recording ? ColorRGBA(1.0f, 0.35f, 0.35f, 1.0f) : ColorRGBA(0.65f, 0.9f, 1.0f, 1.0f));
	TextRender()->Text(X + 4.0f, Y + 4.0f, 5.0f, aLine1, -1.0f);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.95f);
	TextRender()->Text(X + 4.0f, Y + 16.0f, 4.8f, aLine2, -1.0f);
	TextRender()->Text(X + 4.0f, Y + 27.0f, 4.6f, aLine3, -1.0f);
	if(pTasChar != nullptr)
	{
		char aLine4[96];
		str_format(aLine4, sizeof(aLine4), "G:%d FR:%d", pTasChar->IsGrounded() ? 1 : 0, pTasChar->m_FreezeTime);
		TextRender()->Text(X + 4.0f, Y + 38.0f, 4.6f, aLine4, -1.0f);
	}
	if(m_Paused)
		TextRender()->Text(X + 4.0f, Y + 50.0f, 4.6f, "Paused", -1.0f);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CMalinkaTas::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const CScreenRect SavedScreen = Graphics()->GetScreen();
	Graphics()->MapScreenToInterface(GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y, GameClient()->m_Camera.m_Zoom);
	RenderStartEndPos();
	RenderReplayPath();
	RenderPredictionPath();
	RenderHud();
	Graphics()->MapScreen(SavedScreen);
}
