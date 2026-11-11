#ifndef GAME_CLIENT_COMPONENTS_MALINKA_AVOID_POLICY_H
#define GAME_CLIENT_COMPONENTS_MALINKA_AVOID_POLICY_H

// Independent, bounded policy for the documented Avoid controls. This is not
// the closed-source KRX implementation; provenance is in MALINKA_SOURCES.md.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Malinka
{
constexpr int AVOID_MAX_TICKS = 60;
// The pinned documentation screenshot shows Quality 1000. This independent
// ceiling admits that measured setting; it is not a recovered binary maximum.
constexpr int AVOID_MAX_ITERATIONS = 1000;
constexpr int AVOID_MAX_ACTIONS = 6;

enum class EAvoidPrediction
{
	UNAVAILABLE,
	SAFE,
	DANGER,
};

struct SAvoidPrediction
{
	EAvoidPrediction m_State = EAvoidPrediction::UNAVAILABLE;
	int m_DangerTick = 0; // One-based, and meaningful only for DANGER.
};

inline bool AvoidAfkExpired(int64_t Now, int64_t LastActivity, int64_t Frequency, int Seconds)
{
	if(Now < 0 || LastActivity < 0 || Now < LastActivity || Frequency <= 0)
		return false;
	// Divide instead of multiplying a timer frequency by a configurable delay.
	return (Now - LastActivity) / Frequency >= std::clamp(Seconds, 1, 120);
}

inline bool AvoidShouldIntervene(const SAvoidPrediction &Prediction, int KickInTicks)
{
	return Prediction.m_State == EAvoidPrediction::DANGER && Prediction.m_DangerTick > 0 &&
		Prediction.m_DangerTick <= std::clamp(KickInTicks, 1, AVOID_MAX_TICKS);
}

struct SAvoidAction
{
	int m_Direction;
	int m_Hook;
};

struct SAvoidActions
{
	std::array<SAvoidAction, AVOID_MAX_ACTIONS> m_aActions{};
	int m_Count = 0;
};

inline SAvoidActions AvoidActions(int Direction, int Hook, bool AllowDirection, bool AllowHook)
{
	SAvoidActions Result;
	Direction = std::clamp(Direction, -1, 1);
	Hook = Hook != 0;
	Result.m_aActions[Result.m_Count++] = {Direction, Hook}; // User input first.
	for(int Dir = -1; Dir <= 1; ++Dir)
		for(int H = 0; H <= 1; ++H)
			if((AllowDirection || Dir == Direction) && (AllowHook || H == Hook) && (Dir != Direction || H != Hook))
				Result.m_aActions[Result.m_Count++] = {Dir, H};
	return Result;
}

// Signed offsets, all strictly within the chosen FOV. One segment means the
// original aim, rather than an arbitrary edge of the field of view.
inline float AvoidAimOffset(int Index, int Segments, int FovDegrees)
{
	Segments = std::clamp(Segments, 1, 180);
	Index = std::clamp(Index, 0, Segments - 1);
	if(Segments == 1)
		return 0.0f;
	return (static_cast<float>(Index) / (Segments - 1) - 0.5f) * std::clamp(FovDegrees, 1, 360);
}

using CAvoidSequence = std::array<int, AVOID_MAX_TICKS>;

struct SLegitSearchResult
{
	bool m_Available = false;
	bool m_Found = false;
	int m_FirstAction = 0;
	int m_Simulations = 0;
};

// Receding-horizon UCT search. A tree node represents an input prefix, and an
// iteration evaluates one complete sequence in a private world supplied by the
// caller. Quality is an actual simulation budget, Randomness the UCT exploration
// constant, and the three weights reward lifetime and matching user input.
// At most 1001 nodes, 1000 rollouts, 60 inputs/rollout. No allocation or global RNG.
// Any unavailable simulation invalidates this result: failure is never safety.
template<typename FEvaluate>
SLegitSearchResult SearchLegitAvoid(const SAvoidActions &Actions, int Ticks, int Quality, int Randomness,
	int DirectionWeight, int HookWeight, int LifeWeight, uint32_t Seed,
	const SAvoidPrediction &Baseline, FEvaluate Evaluate)
{
	SLegitSearchResult Result;
	if(Actions.m_Count < 1 || Actions.m_Count > AVOID_MAX_ACTIONS || Baseline.m_State == EAvoidPrediction::UNAVAILABLE)
		return Result;
	Ticks = std::clamp(Ticks, 1, AVOID_MAX_TICKS);
	Quality = std::clamp(Quality, 1, AVOID_MAX_ITERATIONS);
	if(Baseline.m_State == EAvoidPrediction::DANGER && (Baseline.m_DangerTick < 1 || Baseline.m_DangerTick > Ticks))
		return Result;
	Result.m_Available = true;
	if(Baseline.m_State == EAvoidPrediction::SAFE || Actions.m_Count == 1)
		return Result;

	struct SNode
	{
		std::array<int, AVOID_MAX_ACTIONS> m_aChildren{-1, -1, -1, -1, -1, -1};
		int m_Parent = -1;
		int m_Visits = 0;
		double m_Reward = 0.0;
	};
	std::array<SNode, AVOID_MAX_ITERATIONS + 1> aNodes{};
	int NodeCount = 1;
	auto Random = [&Seed]() {
		Seed = Seed * 1664525u + 1013904223u;
		return Seed;
	};
	const double Exploration = std::clamp(Randomness, 0, 64) / 64.0;
	const double DirWeight = std::clamp(DirectionWeight, 0, 1000) / 1000.0;
	const double HWeight = std::clamp(HookWeight, 0, 1000) / 1000.0;
	// Retain the existing 0..100 scale so saved presets keep their relative
	// weight, while admitting the documented Life Priority 298 and higher.
	const double LWeight = std::clamp(LifeWeight, 0, 1000) / 100.0;
	double BestReward = -std::numeric_limits<double>::infinity();
	bool BestSafe = false;
	int BestLifetime = 0;
	for(int Iteration = 0; Iteration < Quality; ++Iteration)
	{
		CAvoidSequence Sequence{};
		int Node = 0;
		int Depth = 0;
		while(Depth < Ticks)
		{
			int Action = -1;
			const int Start = static_cast<int>(Random() % Actions.m_Count);
			for(int Offset = 0; Offset < Actions.m_Count; ++Offset)
			{
				const int Candidate = (Start + Offset) % Actions.m_Count;
				if(aNodes[Node].m_aChildren[Candidate] == -1)
				{
					Action = Candidate;
					break;
				}
			}
			if(Action != -1)
			{
				const int Child = NodeCount++;
				aNodes[Node].m_aChildren[Action] = Child;
				aNodes[Child].m_Parent = Node;
				Node = Child;
				Sequence[Depth++] = Action;
				break; // One expansion per iteration bounds the tree.
			}
			double BestUct = -std::numeric_limits<double>::infinity();
			for(int Candidate = 0; Candidate < Actions.m_Count; ++Candidate)
			{
				const SNode &Child = aNodes[aNodes[Node].m_aChildren[Candidate]];
				const double Uct = Child.m_Reward / Child.m_Visits +
					Exploration * std::sqrt(2.0 * std::log(static_cast<double>(aNodes[Node].m_Visits + 1)) / Child.m_Visits);
				if(Uct > BestUct)
				{
					BestUct = Uct;
					Action = Candidate;
				}
			}
			Sequence[Depth++] = Action;
			Node = aNodes[Node].m_aChildren[Action];
		}
		// Prefer holding an action in a rollout, without preventing exploration
		// of a later release or change. This is not an OS/input injection loop.
		for(int Step = Depth; Step < Ticks; ++Step)
			Sequence[Step] = Random() % 4 == 0 ? static_cast<int>(Random() % Actions.m_Count) : Sequence[Step - 1];
		const SAvoidPrediction Prediction = Evaluate(Sequence, Ticks);
		++Result.m_Simulations;
		if(Prediction.m_State == EAvoidPrediction::UNAVAILABLE ||
			(Prediction.m_State == EAvoidPrediction::DANGER && (Prediction.m_DangerTick < 1 || Prediction.m_DangerTick > Ticks)))
		{
			Result.m_Available = Result.m_Found = false;
			return Result;
		}
		const bool Safe = Prediction.m_State == EAvoidPrediction::SAFE;
		const int Lifetime = Safe ? Ticks : Prediction.m_DangerTick - 1;
		int MatchingDirection = 0, MatchingHook = 0;
		for(int Step = 0; Step < Lifetime; ++Step)
		{
			MatchingDirection += Actions.m_aActions[Sequence[Step]].m_Direction == Actions.m_aActions[0].m_Direction;
			MatchingHook += Actions.m_aActions[Sequence[Step]].m_Hook == Actions.m_aActions[0].m_Hook;
		}
		const double Reward = (LWeight * Lifetime + DirWeight * MatchingDirection + HWeight * MatchingHook) / Ticks;
		for(int Current = Node; Current != -1; Current = aNodes[Current].m_Parent)
		{
			++aNodes[Current].m_Visits;
			aNodes[Current].m_Reward += Reward;
		}
		// Safety first among fully surviving sequences; otherwise only accept a
		// sequence that outlives the user's current input, never a shorter one.
		if((Safe || Lifetime >= Baseline.m_DangerTick) &&
			(!Result.m_Found || (Safe && !BestSafe) || (Safe == BestSafe &&
				(Reward > BestReward || (Reward == BestReward && Lifetime > BestLifetime)))))
		{
			Result.m_Found = true;
			Result.m_FirstAction = Sequence[0];
			BestReward = Reward;
			BestSafe = Safe;
			BestLifetime = Lifetime;
		}
	}
	return Result;
}
}
#endif
