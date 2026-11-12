#pragma once

#include <game/collision.h>
#include <game/layers.h>
#include <game/mapitems.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace avoidfreeze
{
inline constexpr std::size_t MAX_STATIC_MAP_TILES = 262144;

struct TileMapSnapshot
{
    int width{};
    int height{};
    std::vector<CTile> game;
    std::vector<CTile> front;
};

bool IsSupportedTileIndex(int index) noexcept;
void ValidateStaticMap(const TileMapSnapshot &snapshot);

class PhysicsMap final
{
    struct Impl;
    std::unique_ptr<Impl> m_Impl;

public:
    explicit PhysicsMap(const TileMapSnapshot &snapshot);
    ~PhysicsMap();
    PhysicsMap(PhysicsMap &&) noexcept;
    PhysicsMap &operator=(PhysicsMap &&) noexcept;
    PhysicsMap(const PhysicsMap &) = delete;
    PhysicsMap &operator=(const PhysicsMap &) = delete;

    CCollision &Collision() noexcept;
    const CCollision &Collision() const noexcept;
};
} // namespace avoidfreeze
