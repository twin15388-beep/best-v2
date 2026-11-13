#include "../avoid_physics.h"
#include <iostream>
#include <vector>
#include <cassert>

using namespace avoidfreeze::proxy;

int main()
{
    // Build a 10x10 test map
    // Row 5 has a freeze tile at (6, 5) and (7, 5)
    constexpr int map_w = 10;
    constexpr int map_h = 10;
    std::vector<Tile> map_tiles(map_w * map_h, Tile{TILE_AIR, 0, 0, 0});

    // Place freeze at x = 6, y = 5
    map_tiles[5 * map_w + 6].m_Index = TILE_FREEZE;
    map_tiles[5 * map_w + 7].m_Index = TILE_FREEZE;

    CollisionView col;
    col.width = map_w;
    col.height = map_h;
    col.tiles = map_tiles.data();
    col.front = nullptr;

    assert(col.IsValid());

    // Test 1: Player at (3 * 32, 5 * 32), standing still. Current dir = 0.
    // Far from freeze, should not predict hazard.
    Vec2 pos1(3.0f * 32.0f, 5.0f * 32.0f);
    Vec2 vel1(0.0f, 0.0f);
    int risk1 = PredictHazard(col, pos1, vel1, 0, 15);
    assert(risk1 == 0);
    int safe1 = ResolveSafeDirection(col, pos1, vel1, 0);
    assert(safe1 == 0);
    std::cout << "[PASS] Test 1: Stationary safe position" << std::endl;

    // Test 2: Player at (5 * 32, 5 * 32), moving right (dir = +1) towards freeze at x = 6.
    // PredictHazard should detect freeze ahead!
    Vec2 pos2(5.0f * 32.0f, 5.0f * 32.0f);
    Vec2 vel2(4.0f, 0.0f);
    int risk2 = PredictHazard(col, pos2, vel2, 1, 15);
    assert(risk2 > 0);
    std::cout << "[PASS] Test 2: Hazard predicted ahead at tick " << risk2 << std::endl;

    // Test 3: ResolveSafeDirection for Test 2 should turn player left (-1) or stop (0).
    int safe2 = ResolveSafeDirection(col, pos2, vel2, 1);
    assert(safe2 <= 0); // Must NOT be +1!
    std::cout << "[PASS] Test 3: Safe direction resolved to " << safe2 << " (diverted from +1)" << std::endl;

    // Test 4: Player moving left away from freeze (dir = -1). Should be safe!
    int risk4 = PredictHazard(col, pos2, Vec2(-2.0f, 0.0f), -1, 15);
    assert(risk4 == 0);
    int safe4 = ResolveSafeDirection(col, pos2, Vec2(-2.0f, 0.0f), -1);
    assert(safe4 == -1);
    std::cout << "[PASS] Test 4: Moving away from hazard preserved dir = -1" << std::endl;

    std::cout << "All isolated physics checks passed successfully!" << std::endl;
    return 0;
}
