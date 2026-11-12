#include "policy/avoid_policy.h"

#include <game/mapitems.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
int g_Checks = 0;

void Check(bool condition, std::string_view message)
{
    ++g_Checks;
    if(!condition)
        throw std::runtime_error(std::string(message));
}

avoidfreeze::SnapshotFrame BasicFrame()
{
    avoidfreeze::SnapshotFrame frame;
    frame.map.width = 10;
    frame.map.height = 6;
    frame.map.game.resize(static_cast<std::size_t>(frame.map.width * frame.map.height));
    for(int x = 0; x < frame.map.width; ++x)
        frame.map.game[static_cast<std::size_t>(4 * frame.map.width + x)].m_Index = TILE_SOLID;

    frame.player.client_id = 0;
    frame.player.tick = 1000;
    frame.player.position = {80.0f, 100.0f};
    frame.player.velocity = {0.0f, 0.0f};
    frame.player.hook_position = frame.player.position;
    frame.player.hook_direction = {1.0f, 0.0f};
    frame.player.hook_tele_base = frame.player.position;
    frame.player.hook_state = HOOK_IDLE;
    frame.player.active_weapon = WEAPON_GUN;
    frame.player.jumps = 2;
    frame.player.direction = 0;
    frame.player.input.m_TargetX = 1;
    frame.player.input.m_TargetY = 0;
    frame.player.tuning = CTuningParams{};

    frame.checks = {
        true, // exact_public_image
        true, // native_layout_matches
        true, // unique_root
        true, // online
        true, // local_ids_match
        true, // one_active_player
        true, // no_other_prediction_entities
        true, // no_unsupported_collision_layers
        true, // no_ego_tile_prediction
        true, // supported_prediction_mode
        true  // stable_double_read
    };
    return frame;
}

void TestCleanPath()
{
    auto frame = BasicFrame();
    const auto decision = avoidfreeze::Analyze(frame);
    if(decision.kind != avoidfreeze::DecisionKind::no_predicted_hazard)
        throw std::runtime_error("clear map decision failed: " + decision.reason);
    Check(true, "clear map should have no predicted hazard");
}

void TestAvoidableFreezeTile()
{
    auto frame = BasicFrame();
    frame.map.game[static_cast<std::size_t>(3 * frame.map.width + 4)].m_Index = TILE_FREEZE;
    frame.player.direction = 1;
    frame.player.input.m_Direction = 1;
    const auto decision = avoidfreeze::Analyze(frame);
    Check(decision.kind == avoidfreeze::DecisionKind::suggested_direction, "rightward freeze path should have a safe advice candidate");
    Check(decision.predicted_hazard_tick > 0 && decision.predicted_hazard_tick <= avoidfreeze::PREDICTION_HORIZON_TICKS,
          "baseline hazard tick should be within the bounded horizon");
    Check(decision.suggested_direction != 1, "suggestion must differ from the hazardous current direction");
}

void TestAlreadyOnHazardHasNoFalseSafeAdvice()
{
    auto frame = BasicFrame();
    frame.map.game[static_cast<std::size_t>(3 * frame.map.width + 2)].m_Index = TILE_FREEZE;
    const auto decision = avoidfreeze::Analyze(frame);
    Check(decision.kind == avoidfreeze::DecisionKind::no_safe_direction,
          "current hazard tile must not be reported as a safe corrective action");
}

void TestAlreadyFrozen()
{
    auto frame = BasicFrame();
    frame.player.freeze_time = 5;
    const auto decision = avoidfreeze::Analyze(frame);
    Check(decision.kind == avoidfreeze::DecisionKind::already_frozen, "positive freeze timer should stop advice");
}

void TestUnsupportedCaptureFailsClosed()
{
    auto frame = BasicFrame();
    frame.checks.unique_root = false;
    const auto decision = avoidfreeze::Analyze(frame);
    Check(decision.kind == avoidfreeze::DecisionKind::unavailable, "ambiguous root must fail closed");
}

void TestUnsupportedTileFailsClosed()
{
    auto frame = BasicFrame();
    frame.map.game[0].m_Index = TILE_TELEIN;
    const auto decision = avoidfreeze::Analyze(frame);
    Check(decision.kind == avoidfreeze::DecisionKind::unavailable, "teleporter tile must fail closed");
}

void TestMalformedGeometryFailsClosed()
{
    auto frame = BasicFrame();
    frame.map.front.emplace_back();
    const auto decision = avoidfreeze::Analyze(frame);
    Check(decision.kind == avoidfreeze::DecisionKind::unavailable, "truncated front layer must fail closed");
}
} // namespace

int main()
{
    try
    {
        TestCleanPath();
        TestAvoidableFreezeTile();
        TestAlreadyOnHazardHasNoFalseSafeAdvice();
        TestAlreadyFrozen();
        TestUnsupportedCaptureFailsClosed();
        TestUnsupportedTileFailsClosed();
        TestMalformedGeometryFailsClosed();
    }
    catch(const std::exception &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "PASS: " << g_Checks << " isolated policy assertions; no game process or input used\n";
    return EXIT_SUCCESS;
}
