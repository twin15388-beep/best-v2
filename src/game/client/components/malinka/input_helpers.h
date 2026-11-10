#ifndef GAME_CLIENT_COMPONENTS_MALINKA_INPUT_HELPERS_H
#define GAME_CLIENT_COMPONENTS_MALINKA_INPUT_HELPERS_H

#include <base/vmath.h>
#include <generated/protocol.h>

#include <algorithm>
#include <cmath>

namespace Malinka
{
inline vec2 FiniteAim(vec2 Aim)
{
	if(!std::isfinite(Aim.x) || !std::isfinite(Aim.y))
		return vec2(1.0f, 0.0f);
	// Keep float-to-network-int conversion representable, preserving direction.
	constexpr float MaxComponent = 1000000.0f;
	const float Component = std::max(std::abs(Aim.x), std::abs(Aim.y));
	if(Component < 0.001f)
		return vec2(1.0f, 0.0f);
	if(Component < 1.0f)
		Aim /= Component; // Quantization to network integers must not produce (0, 0).
	if(Component > MaxComponent)
		Aim *= MaxComponent / Component;
	return Aim;
}

inline CNetObj_PlayerInput NeutralInput(const CNetObj_PlayerInput &Observed, const CNetObj_PlayerInput &LastSent)
{
	CNetObj_PlayerInput Result = Observed;
	Result.m_Direction = 0;
	Result.m_Jump = 0;
	Result.m_Hook = 0;
	// Fire and weapon changes are edge counters, not booleans. Resetting them to
	// zero would wrap the counter and could create unintended presses.
	Result.m_Fire = (LastSent.m_Fire + (LastSent.m_Fire & 1)) & INPUT_STATE_MASK;
	Result.m_WantedWeapon = 0;
	Result.m_NextWeapon = LastSent.m_NextWeapon;
	Result.m_PrevWeapon = LastSent.m_PrevWeapon;
	const vec2 Aim = FiniteAim(vec2(Result.m_TargetX, Result.m_TargetY));
	Result.m_TargetX = static_cast<int>(Aim.x);
	Result.m_TargetY = static_cast<int>(Aim.y);
	return Result;
}

// Translate logical button counters to the actual network stream. A domain
// change (live / recording / replay) must not replay accumulated local presses.
class CInputStream
{
	CNetObj_PlayerInput m_LastSource{};
	CNetObj_PlayerInput m_LastSent{};
	bool m_Initialized = false;
	bool m_WasIsolated = false;
	int m_Domain = 0;

public:
	CNetObj_PlayerInput Filter(const CNetObj_PlayerInput &Source, bool Isolate, int Domain)
	{
		CNetObj_PlayerInput Out = Source;
		const bool Rebase = !m_Initialized || m_WasIsolated || m_Domain != Domain;
		if(Isolate)
			Out = NeutralInput(Source, m_LastSent);
		else
		{
			const int FireDelta = Rebase ? ((Source.m_Fire & 1) != (m_LastSent.m_Fire & 1)) :
				((Source.m_Fire - m_LastSource.m_Fire) & INPUT_STATE_MASK);
			Out.m_Fire = (m_LastSent.m_Fire + FireDelta) & INPUT_STATE_MASK;
			Out.m_NextWeapon = (m_LastSent.m_NextWeapon + (Rebase ? 0 : ((Source.m_NextWeapon - m_LastSource.m_NextWeapon) & INPUT_STATE_MASK))) & INPUT_STATE_MASK;
			Out.m_PrevWeapon = (m_LastSent.m_PrevWeapon + (Rebase ? 0 : ((Source.m_PrevWeapon - m_LastSource.m_PrevWeapon) & INPUT_STATE_MASK))) & INPUT_STATE_MASK;
		}
		m_LastSource = Source;
		m_LastSent = Out;
		m_Initialized = true;
		m_WasIsolated = Isolate;
		m_Domain = Domain;
		return Out;
	}
};
}

#endif
