#include "static_map.h"

#include <engine/map.h>

#include <bit>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace avoidfreeze
{
namespace
{
class OwnedMap final : public IMap
{
    CMapItemGroup m_Group{};
    std::vector<CMapItemLayerTilemap> m_Layers;
    std::vector<std::vector<CTile>> m_Data;

    void AddLayer(int flags, const std::vector<CTile> &tiles, int width, int height)
    {
        if(m_Layers.size() >= 2 || tiles.empty())
            throw std::invalid_argument("static map layer count/data is invalid");

        CMapItemLayerTilemap layer{};
        layer.m_Layer.m_Type = LAYERTYPE_TILES;
        layer.m_Version = 3;
        layer.m_Width = width;
        layer.m_Height = height;
        layer.m_Flags = flags;
        layer.m_Color.r = 255;
        layer.m_Color.g = 255;
        layer.m_Color.b = 255;
        layer.m_Color.a = 255;
        layer.m_ColorEnv = -1;
        layer.m_Image = -1;
        layer.m_Data = static_cast<int>(m_Data.size());
        layer.m_Tele = -1;
        layer.m_Speedup = -1;
        layer.m_Front = -1;
        layer.m_Switch = -1;
        layer.m_Tune = -1;
        m_Data.push_back(tiles);
        m_Layers.push_back(layer);
    }

public:
    explicit OwnedMap(const TileMapSnapshot &snapshot)
    {
        static_assert(std::endian::native == std::endian::little);
        m_Group.m_Version = 3;
        m_Group.m_OffsetX = 0;
        m_Group.m_OffsetY = 0;
        m_Group.m_ParallaxX = 100;
        m_Group.m_ParallaxY = 100;
        m_Group.m_StartLayer = 0;
        m_Group.m_UseClipping = 0;
        m_Group.m_ClipX = 0;
        m_Group.m_ClipY = 0;
        m_Group.m_ClipW = 0;
        m_Group.m_ClipH = 0;

        AddLayer(TILESLAYERFLAG_GAME, snapshot.game, snapshot.width, snapshot.height);
        if(!snapshot.front.empty())
            AddLayer(TILESLAYERFLAG_FRONT, snapshot.front, snapshot.width, snapshot.height);
        m_Group.m_NumLayers = static_cast<int>(m_Layers.size());
    }

    int GetDataSize(int index) const override
    {
        const auto &data = m_Data.at(static_cast<std::size_t>(index));
        return static_cast<int>(data.size() * sizeof(CTile));
    }
    void *GetData(int index) override
    {
        return m_Data.at(static_cast<std::size_t>(index)).data();
    }
    void *GetDataSwapped(int index) override { return GetData(index); }
    const char *GetDataString(int) override { throw std::logic_error("owned map contains no strings"); }
    void UnloadData(int) override {}
    int NumData() const override { return static_cast<int>(m_Data.size()); }

    int GetItemSize(int index) override
    {
        if(index == 0)
            return static_cast<int>(sizeof(m_Group));
        if(index > 0 && static_cast<std::size_t>(index) <= m_Layers.size())
            return static_cast<int>(sizeof(CMapItemLayerTilemap));
        throw std::out_of_range("owned map item index");
    }
    void *GetItem(int index, int *type = nullptr, int *id = nullptr, CUuid * = nullptr) override
    {
        if(index < 0 || index >= NumItems())
            throw std::out_of_range("owned map item index");
        if(type != nullptr)
            *type = index == 0 ? MAPITEMTYPE_GROUP : MAPITEMTYPE_LAYER;
        if(id != nullptr)
            *id = index == 0 ? 0 : index - 1;
        return index == 0 ? static_cast<void *>(&m_Group) : static_cast<void *>(&m_Layers.at(static_cast<std::size_t>(index - 1)));
    }
    void GetType(int type, int *start, int *count) override
    {
        if(start == nullptr || count == nullptr)
            throw std::invalid_argument("owned map type output pointer is null");
        *start = 0;
        *count = 0;
        if(type == MAPITEMTYPE_GROUP)
            *count = 1;
        else if(type == MAPITEMTYPE_LAYER)
        {
            *start = 1;
            *count = static_cast<int>(m_Layers.size());
        }
    }
    int FindItemIndex(int type, int id) override
    {
        if(type == MAPITEMTYPE_GROUP && id == 0)
            return 0;
        if(type == MAPITEMTYPE_LAYER && id >= 0 && static_cast<std::size_t>(id) < m_Layers.size())
            return id + 1;
        return -1;
    }
    void *FindItem(int type, int id) override
    {
        const int index = FindItemIndex(type, id);
        return index < 0 ? nullptr : GetItem(index);
    }
    int NumItems() const override { return static_cast<int>(m_Layers.size()) + 1; }

    bool Load(const char *, IStorage *, const char *, int) override { return false; }
    bool Load(IStorage *, const char *, int) override { return false; }
    void Unload() override {}
    bool IsLoaded() const override { return true; }
    IOHANDLE File() const override { return nullptr; }
    const char *FullName() const override { return "avoid-freeze-owned-static-map"; }
    const char *BaseName() const override { return FullName(); }
    const char *Path() const override { return FullName(); }
    SHA256_DIGEST Sha256() const override { throw std::logic_error("owned map has no serialized identity"); }
    unsigned Crc() const override { throw std::logic_error("owned map has no serialized CRC"); }
    int Size() const override { throw std::logic_error("owned map has no serialized size"); }
};
} // namespace

struct PhysicsMap::Impl
{
    OwnedMap map;
    CLayers layers;
    CCollision collision;

    explicit Impl(const TileMapSnapshot &snapshot) : map(snapshot)
    {
        layers.Init(&map, false, false);
        collision.Init(&layers);
    }
};

bool IsSupportedTileIndex(int index) noexcept
{
    switch(index)
    {
    case TILE_AIR:
    case TILE_SOLID:
    case TILE_DEATH:
    case TILE_NOHOOK:
    case TILE_FREEZE:
    case TILE_UNFREEZE:
    case TILE_DFREEZE:
    case TILE_DUNFREEZE:
    case TILE_LFREEZE:
    case TILE_LUNFREEZE:
    case TILE_START:
    case TILE_FINISH:
        return true;
    case ENTITY_OFFSET + ENTITY_SPAWN:
    case ENTITY_OFFSET + ENTITY_SPAWN_RED:
    case ENTITY_OFFSET + ENTITY_SPAWN_BLUE:
        return true;
    default:
        return false;
    }
}

void ValidateStaticMap(const TileMapSnapshot &snapshot)
{
    if(snapshot.width <= 0 || snapshot.height <= 0)
        throw std::invalid_argument("map dimensions must be positive");
    const auto width = static_cast<std::uint64_t>(snapshot.width);
    const auto height = static_cast<std::uint64_t>(snapshot.height);
    if(width > MAX_STATIC_MAP_TILES / height)
        throw std::invalid_argument("map exceeds the bounded static tile budget");
    const auto cells = static_cast<std::size_t>(width * height);
    if(snapshot.game.size() != cells || (!snapshot.front.empty() && snapshot.front.size() != cells))
        throw std::invalid_argument("game/front tile counts do not match map dimensions");

    auto check_layer = [](const std::vector<CTile> &layer) {
        for(const CTile &tile : layer)
        {
            if(tile.m_MustBe0 != 0 || !IsSupportedTileIndex(tile.m_Index))
                throw std::invalid_argument("map contains a reserved or unsupported tile index");
        }
    };
    check_layer(snapshot.game);
    check_layer(snapshot.front);
}

PhysicsMap::PhysicsMap(const TileMapSnapshot &snapshot)
{
    ValidateStaticMap(snapshot);
    m_Impl = std::make_unique<Impl>(snapshot);
}

PhysicsMap::~PhysicsMap() = default;
PhysicsMap::PhysicsMap(PhysicsMap &&) noexcept = default;
PhysicsMap &PhysicsMap::operator=(PhysicsMap &&) noexcept = default;
CCollision &PhysicsMap::Collision() noexcept { return m_Impl->collision; }
const CCollision &PhysicsMap::Collision() const noexcept { return m_Impl->collision; }
} // namespace avoidfreeze
