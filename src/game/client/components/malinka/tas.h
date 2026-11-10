// Recording UI/rendering adapted from Pasta-Client; simulation and I/O rewritten.
// See docs/MALINKA_SOURCES.md and license.txt.
#ifndef GAME_CLIENT_COMPONENTS_MALINKA_TAS_H
#define GAME_CLIENT_COMPONENTS_MALINKA_TAS_H

#include "replay_metadata.h"

#include <generated/protocol.h>
#include <engine/shared/protocol.h>
#include <engine/client/enums.h>

#include <game/client/component.h>
#include <game/client/prediction/gameworld.h>

#include <base/vmath.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

class CMalinkaTas : public CComponent
{
public:
	struct SFrame
	{
		int m_Tick = 0;
		CNetObj_PlayerInput m_Input{};
		vec2 m_Mouse{};
		vec2 m_Pos{}; // position BEFORE this frame's input
		bool m_HasDummy = false;
		CNetObj_PlayerInput m_DummyInput{};
		vec2 m_DummyPos{};
	};

	CMalinkaTas() = default;
	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override;
	void OnShutdown() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnUpdate() override;
	void OnRender() override;

	void ObserveInput(const CNetObj_PlayerInput &Input, vec2 Pos);
	void ObserveSlotInput(int Slot, const CNetObj_PlayerInput &Input);
	// Called at the final network boundary, after dummy hammer/copy and Best's
	// fast-practice filter. Returns 0=live, 1=local recording, 2=playback,
	// 3=paused/stopping playback. Modes 1 and 3 require neutral network input.
	int PrepareNetworkInput(CNetObj_PlayerInput &Input, int Slot, int PredTick);
	bool NeedsNetworkInput() const;
	bool GetPlaybackInput(CNetObj_PlayerInput &Out, int Slot = -1) const;
	bool HasRenderMouseOverride() const;
	vec2 GetRenderMousePos() const;
	bool IsWorldActive() const;
	bool IsWorldInitialized() const { return m_WorldInitialized; }
	vec2 GetInterpolatedPlayerPos() const;

	void SaveReplay();
	void LoadSelectedReplay();
	void ValidateReplay();
	void ReportReplayTime();
	void SyncNow();

	bool IsRecording() const { return m_Recording; }
	bool IsPlaying() const { return m_Playing; }
	bool IsPaused() const { return m_Paused; }
	int NumFrames() const { return static_cast<int>(m_vFrames.size()); }
	int PlaybackCursor() const { return m_PlayCursor; }
	float GetStepRate() const;
	void RenderWorldCharacters();

private:
	std::vector<SFrame> m_vFrames;
	Malinka::Replay::SMetadata m_Metadata;
	std::unique_ptr<CGameWorld> m_pInitialWorld;
	std::unique_ptr<CGameWorld> m_pWorld;
	std::unique_ptr<CGameWorld> m_pWorldPredicted;
	std::unique_ptr<CGameWorld> m_pValidationWorld;
	std::array<CNetObj_PlayerInput, NUM_DUMMIES> m_aObservedInputs{};
	std::array<bool, NUM_DUMMIES> m_aHaveInput{};
	std::array<int, NUM_DUMMIES> m_aWorldClientIds = {-1, -1};
	int m_PrimarySlot = 0;
	int m_PlayCursor = 0;
	int m_LastPlaybackTick = -1;
	int m_SeekCursor = 0;
	int m_SeekTarget = -1;
	int m_ValidationCursor = 0;
	int64_t m_LastUpdateTime = 0;
	int64_t m_LastAutoSave = 0;
	int64_t m_LastSeekTime = 0;
	bool m_Recording = false;
	bool m_Playing = false;
	bool m_Paused = false;
	bool m_WorldInitialized = false;
	bool m_HaveObservedInput = false;
	bool m_LatchedJump = false;
	bool m_WasFrozenInWorld = false;
	bool m_WasRewind = false;
	bool m_WasForward = false;
	bool m_AutoForwardActive = false;
	bool m_WaitingForReplayStart = false;
	bool m_ReleasePlaybackInput = false;
	bool m_ValidationForPlayback = false;
	double m_Accumulator = 0.0;
	float m_SmoothIntra = 0.0f;
	CNetObj_PlayerInput m_ObservedInput{};
	vec2 m_ObservedMouse{};
	vec2 m_ObservedPos{};
	mutable std::string m_LastIoError;

	bool InteractionAllowed() const;
	bool CurrentMapMatches() const;
	void SetCurrentMapMetadata();
	void ClearReplay(bool KillPlayer);
	void StartRecording();
	void StopRecording();
	bool StartPlayback();
	void StopPlayback();
	bool AtReplayStart() const;
	void EnsureWorldInitialized();
	void ShutdownWorld();
	void ResetWorldFromPredicted();
	void RewindRecording(int Amount);
	void StepForwardRecording();
	void AutoForwardRecording();
	void RunSeekSlice();
	void RunValidationSlice();
	bool IsLocalFrozenInWorld() const;
	[[nodiscard]] bool ApplyFrame(CGameWorld &World, const SFrame &Frame, bool EmitEvents);
	void SimulationFailed();
	void UpdatePredictedWorld(const CNetObj_PlayerInput &Input, vec2 Mouse);
	void DrainEvents(CGameWorld &World, bool EmitEvents);
	bool SaveReplayToPath(const char *pPath) const;
	bool LoadReplayFromPath(const char *pPath);
	bool FindReplayPath(char *pBuf, int BufSize, bool ExactStemOnly) const;
	bool GetReplaySavePath(char *pBuf, int BufSize, bool AutoSave) const;
	void PushTasWarning(const char *pTitle, const char *pMessage) const;
	void RenderTasWorld();
	void RenderStartEndPos();
	void RenderReplayPath();
	void RenderPredictionPath();
	void RenderHud();
};

#endif
