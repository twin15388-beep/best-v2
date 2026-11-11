// Canonical private-world input ordering, matching Best's prediction loop.
#ifndef GAME_CLIENT_COMPONENTS_MALINKA_SIMULATION_STEP_H
#define GAME_CLIENT_COMPONENTS_MALINKA_SIMULATION_STEP_H

#include <engine/client/enums.h>

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

#include <algorithm>
#include <array>
#include <exception>
#include <initializer_list>
#include <limits>

namespace Malinka
{
struct SSimulationInput
{
	int m_ClientId;
	CNetObj_PlayerInput m_Input;
};

// Call only on an owned private world. -1 means an absent optional dummy.
// Validate the whole input set before mutation. Reacquire characters after
// direct input/ticks, which can destroy entities; no cached pointer escapes.
[[nodiscard]] inline bool TickSimulation(CGameWorld &World, std::initializer_list<SSimulationInput> Inputs) try
{
	if(Inputs.size() > NUM_DUMMIES || !World.Collision() || !World.TuningList() || World.m_GameTick == std::numeric_limits<int>::max())
		return false;
	std::array<SSimulationInput, NUM_DUMMIES> aSorted;
	size_t Count = 0;
	for(const auto &Input : Inputs)
	{
		if(Input.m_ClientId == -1)
			continue;
		if(!World.GetCharacterById(Input.m_ClientId))
			return false;
		aSorted[Count++] = Input;
	}
	std::sort(aSorted.begin(), aSorted.begin() + Count, [](const auto &A, const auto &B) { return A.m_ClientId < B.m_ClientId; });
	for(size_t Index = 1; Index < Count; ++Index)
		if(aSorted[Index - 1].m_ClientId == aSorted[Index].m_ClientId)
			return false;
	for(size_t Index = 0; Index < Count; ++Index)
		if(auto *pCharacter = World.GetCharacterById(aSorted[Index].m_ClientId))
			pCharacter->OnDirectInput(&aSorted[Index].m_Input);
	++World.m_GameTick;
	for(const auto &Input : Inputs)
		if(auto *pCharacter = World.GetCharacterById(Input.m_ClientId))
			pCharacter->OnPredictedInput(&Input.m_Input);
	World.Tick();
	return true;
}
catch(const std::exception &)
{
	return false; // Discard/stop this private simulation on failure.
}
}
#endif
