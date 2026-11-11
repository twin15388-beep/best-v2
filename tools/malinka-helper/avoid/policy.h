// SPDX-License-Identifier: MIT
#pragma once
#include "physics_import.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace malinka::avoid
{
inline constexpr std::size_t MAX_PREVIEW_TILES=262144;
inline constexpr int HORIZON_TICKS=40, INPUT_DELAY_TICKS=3;
struct StaticMap
{
    int width{},height{};
    std::uint64_t generation{};
    std::vector<std::uint8_t> game,front;
};
// Exactly the g_Config fields used by the pinned prediction source. They are
// client-side prediction settings, NOT a claimed export of server settings.
inline constexpr std::size_t CONFIG_COUNT=13;
struct Frame
{
    int client_id{-1},dummy{-1},tick{-1};
    std::uint64_t measured_ms{};
    CNetObj_PlayerInput controls{};
    Replica::WorldFlags flags{};
    std::array<int,CONFIG_COUNT> config{};
    std::vector<std::uint8_t> character;
    std::shared_ptr<const StaticMap> map;
};
enum class Transition { learning, matched, mismatch };
Transition CheckTransition(const Frame &before,const Frame &after);
bool SupportedStaticMapTile(int tile);
enum class DecisionState { no_danger, correcting, already_frozen, no_safe_correction, unavailable };
struct Decision
{
    DecisionState state{DecisionState::unavailable};
    int direction{},baseline_danger_tick{};
    std::string reason;
};
// allowed[] corresponds to {-1,0,+1}. The input host must exclude any direction
// requiring a key-up of a physically held key; the policy cannot send input.
Decision Choose(const Frame &frame,int physical_direction,const std::array<bool,3> &allowed);
} // namespace malinka::avoid
