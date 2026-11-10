// Movement probes derived from Pasta-Client dd7f5cd8c73ecb7259dba4f9dd9a7134babc890f.
// See docs/MALINKA_SOURCES.md and license.txt.
#ifndef GAME_CLIENT_COMPONENTS_MALINKA_AVOIDFREEZE_H
#define GAME_CLIENT_COMPONENTS_MALINKA_AVOIDFREEZE_H

#include "avoid_policy.h"

#include <base/vmath.h>
#include <engine/client/enums.h>
#include <game/client/component.h>
#include <generated/protocol.h>

class CCharacter;

class CMalinkaAvoidFreeze : public CComponent
{
	CNetObj_PlayerInput m_aLastSafeInput[NUM_DUMMIES]{};
	bool m_aLastSafeValid[NUM_DUMMIES]{};
	int m_aLastSafeClientId[NUM_DUMMIES]{};
	int m_aLastMode[NUM_DUMMIES]{};

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void ApplyAvoid(CNetObj_PlayerInput &Input, int64_t LastActivity);

	bool IsFreezeTile(int Tile) const;
	bool HasFreezeTile(vec2 Pos) const;
	bool HasFreezeAbove(vec2 Pos, int Tiles);
	int DetectFreezeSide(vec2 Pos);
	bool ShouldAutoJumpSave(CCharacter *pCharacter);
	bool HasPlayerNearAim(const CCharacter *pLocalCharacter, vec2 AimDir, float MaxDistance, float MaxAngleRadians);
	bool HasImmediateSideFreeze(const CCharacter *pCharacter, int Direction);
	bool HasImpendingFreeze(const CCharacter *pCharacter, int Ticks);
	bool IsBlatantFrozenAtPos(vec2 Pos);

	bool m_BlatantFreezeResetFrozenLast[NUM_DUMMIES]{};
	int64_t m_BlatantFreezeResetSince[NUM_DUMMIES]{};
	mutable vec2 m_AvoidLastMousePos[NUM_DUMMIES]{};
};

#endif
