#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace avoidfreeze::proxy
{

// Standard Teeworlds / DDNet map tiles
enum TileType : uint8_t
{
    TILE_AIR = 0,
    TILE_SOLID = 1,
    TILE_DEATH = 2,
    TILE_NOHOOK = 3,
    TILE_NOLASER = 4,
    TILE_FREEZE = 9,
    TILE_UNFREEZE = 11,
    TILE_DFREEZE = 12,
    TILE_DUNFREEZE = 13,
    TILE_LFREEZE = 144,
    TILE_LUNFREEZE = 145,
};

struct Tile
{
    uint8_t m_Index;
    uint8_t m_Flags;
    uint8_t m_Skip;
    uint8_t m_MustBe0;
};

struct Vec2
{
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() = default;
    constexpr Vec2(float _x, float _y) : x(_x), y(_y) {}

    constexpr Vec2 operator+(const Vec2 &o) const noexcept { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2 &o) const noexcept { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const noexcept { return {x * s, y * s}; }
    constexpr Vec2 &operator+=(const Vec2 &o) noexcept { x += o.x; y += o.y; return *this; }
};

struct CollisionView
{
    int width{0};
    int height{0};
    const Tile *tiles{nullptr};
    const Tile *front{nullptr};

    [[nodiscard]] bool IsValid() const noexcept
    {
        return width > 0 && height > 0 && tiles != nullptr;
    }

    [[nodiscard]] int GetTileIndex(int x, int y) const noexcept
    {
        if(x < 0 || x >= width || y < 0 || y >= height)
            return TILE_AIR;
        const int idx = y * width + x;
        return tiles[idx].m_Index;
    }

    [[nodiscard]] int GetFrontTileIndex(int x, int y) const noexcept
    {
        if(!front || x < 0 || x >= width || y < 0 || y >= height)
            return TILE_AIR;
        const int idx = y * width + x;
        return front[idx].m_Index;
    }
};

[[nodiscard]] constexpr bool IsHazardTile(int tile) noexcept
{
    return tile == TILE_DEATH || tile == TILE_FREEZE || tile == TILE_DFREEZE || tile == TILE_LFREEZE;
}

[[nodiscard]] bool IsPositionHazard(const CollisionView &col, Vec2 pos) noexcept;

[[nodiscard]] int PredictHazard(const CollisionView &col, Vec2 start_pos, Vec2 start_vel, int dir, int max_ticks = 15) noexcept;

[[nodiscard]] int ResolveSafeDirection(const CollisionView &col, Vec2 pos, Vec2 vel, int current_dir) noexcept;

} // namespace avoidfreeze::proxy
