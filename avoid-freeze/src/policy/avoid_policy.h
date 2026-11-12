#pragma once

#include "physics/snapshot.h"

#include <array>
#include <string>

namespace avoidfreeze
{
enum class DecisionKind
{
    unavailable,
    already_frozen,
    immune,
    no_predicted_hazard,
    suggested_direction,
    no_safe_direction
};

struct Decision
{
    DecisionKind kind{DecisionKind::unavailable};
    int current_direction{};
    int suggested_direction{};
    int predicted_hazard_tick{};
    std::string reason;
};

inline constexpr int PREDICTION_HORIZON_TICKS = 40;

// Advice only. This policy does not send keyboard input or access a process.
Decision Analyze(const SnapshotFrame &frame);
} // namespace avoidfreeze
