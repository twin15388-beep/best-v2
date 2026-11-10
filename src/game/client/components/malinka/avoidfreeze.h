// Derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f; modified for BestClient.
// See docs/MALINKA_SOURCES.md and license.txt.
#ifndef GAME_CLIENT_COMPONENTS_MALINKA_AVOIDFREEZE_H
#define GAME_CLIENT_COMPONENTS_MALINKA_AVOIDFREEZE_H

#include <base/vmath.h>
#include <engine/console.h>
#include <game/client/component.h>
#include <generated/protocol.h>
#include <game/client/prediction/gameworld.h>
#include <engine/client/enums.h>
#include <vector>

class CMalinkaAvoidFreeze : public CComponent
{
public:
	virtual int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;

	int MalinkaHookAssistTicks() const;
	int MalinkaDirectionSensitivityStep() const;
	float MalinkaDirectionSensitivityFactor() const;
	int MalinkaMaxAvoidAttempts() const;
	int MalinkaMaxAvoidAttemptsPerDirection() const;
	bool MalinkaGetFreeze(vec2 Pos, int FreezeTime) const;
	bool MalinkaAutomationAllowed() const;
	void MalinkaAvoidFreeze();
	void MalinkaHookAssist();
	bool MalinkaIsAvoidCooldownElapsed(int64_t CurrentTime) const;
	bool IsFreezeTile(int Tile) const;
	bool HasFreezeTile(vec2 Pos) const;
	bool IsDangerousMapIndex(int MapIndex, bool DetectFreeze, bool DetectDeath, bool DetectTele);
	bool MalinkaTryMove(const CNetObj_PlayerInput &BaseInput, int Direction, int CheckTicks);
	bool MalinkaTryAvoidFreeze(int LocalPlayerId);
	int MalinkaAvoidPredictTicks() const;
	void MalinkaUpdateAvoidCooldown(int64_t CurrentTime);
	enum class EFreezePrediction { UNAVAILABLE, SAFE, DANGER };
	EFreezePrediction MalinkaPredictFreeze(const CNetObj_PlayerInput &Input, int Ticks, int *pDangerTick = nullptr, float *pDangerDistance = nullptr) const;
	bool HasDangerousTile(vec2 Pos, bool DetectFreeze, bool DetectDeath, bool DetectTele);
	bool HasFreezeAbove(vec2 Pos, int Tiles);
	int DetectFreezeSide(vec2 Pos);
	bool ShouldAutoJumpSave(CCharacter *pCharacter);
	bool HasPlayerNearAim(const CCharacter *pLocalCharacter, vec2 AimDir, float MaxDistance, float MaxAngleRadians);
	bool HasImmediateSideFreeze(const CCharacter *pCharacter, int Direction);
	bool IsSolidTileAt(int TileX, int TileY);
	bool FindBlatantHookTarget(vec2 Pos, vec2 Vel, vec2 GoalPos, float HookLength, bool DangerAhead, vec2 &OutTarget);
	bool HasImpendingFreeze(const CCharacter *pCharacter, int Ticks);
	bool HasBlatantDangerAhead(const CCharacter *pCharacter, int Direction, int CheckTicks, bool DetectFreeze, bool DetectDeath, bool DetectTele, bool DetectUnfreeze);
	int GetBlatantDangerTick(const CCharacter *pCharacter, int Direction, int CheckTicks, bool DetectFreeze, bool DetectDeath, bool DetectTele, bool DetectUnfreeze);
	int SimulateBlatantFreezeTick(const CNetObj_PlayerInput &BaseInput, int Ticks, bool DetectFreeze, bool DetectDeath, bool DetectTele, bool DetectUnfreeze);
	bool IsBlatantFrozenAtPos(vec2 Pos);

	bool m_AvoidForcing[NUM_DUMMIES];
	bool m_BlatantTrackPointInit[NUM_DUMMIES];
	vec2 m_BlatantTrackPoint[NUM_DUMMIES];
	vec2 m_BlatantHookTarget[NUM_DUMMIES];
	int m_BlatantHookHoldTicks[NUM_DUMMIES];
	bool m_BlatantFreezeResetFrozenLast[NUM_DUMMIES];
	int64_t m_BlatantFreezeResetSince[NUM_DUMMIES];
	int m_AvoidForcedDir[NUM_DUMMIES];
	int64_t m_AvoidForceUntil[NUM_DUMMIES];
	bool m_AvoidWasInDanger[NUM_DUMMIES];
	mutable vec2 m_AvoidLastMousePos[NUM_DUMMIES];

};

#endif
