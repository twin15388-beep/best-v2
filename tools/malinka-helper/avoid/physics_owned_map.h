// An owned, bounded IMap assembled from measured tile arrays. Not a mapped
// view of target memory. CLayers/CCollision are the unmodified pinned sources.
#pragma once
#include <engine/map.h>
#include <game/layers.h>
#include <game/collision.h>
#include <game/mapitems.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

class CPhysicsOwnedMap final : public IMap
{
    CMapItemGroup m_Group{};
    std::vector<CMapItemLayerTilemap> m_Layers;
    std::vector<std::vector<std::uint8_t>> m_Data;
    int m_Width, m_Height;
public:
    CPhysicsOwnedMap(int Width, int Height, const std::vector<std::uint8_t> &Game) : m_Width(Width), m_Height(Height)
    {
        if(Width <= 0 || Height <= 0 || static_cast<std::int64_t>(Width)*Height > 2*1024*1024 || Game.size() != static_cast<std::size_t>(Width)*static_cast<std::size_t>(Height)*sizeof(CTile))
            throw std::runtime_error("Invalid owned tile geometry");
        m_Group.m_Version = 3; m_Group.m_ParallaxX = m_Group.m_ParallaxY = 100;
        AddLayer(TILESLAYERFLAG_GAME, Game);
    }
    void AddLayer(int Flag, const std::vector<std::uint8_t> &Data)
    {
        // This first live adapter deliberately accepts game/front arrays only.
        // Other layer types remain a visible gate, not an approximate world.
        if(m_Layers.size() >= 2 || (Flag != TILESLAYERFLAG_GAME && Flag != TILESLAYERFLAG_FRONT) ||
            Data.size() != static_cast<std::size_t>(m_Width)*static_cast<std::size_t>(m_Height)*sizeof(CTile))
            throw std::runtime_error("Unsupported or malformed owned layer");
        for(const auto &Existing : m_Layers) if(Existing.m_Flags==Flag) throw std::runtime_error("Duplicate owned layer");
        CMapItemLayerTilemap Layer{};
        Layer.m_Version = 3; Layer.m_Layer.m_Type = LAYERTYPE_TILES;
        Layer.m_Width = m_Width; Layer.m_Height = m_Height; Layer.m_Flags = Flag;
        Layer.m_Image = -1; Layer.m_Data = static_cast<int>(m_Data.size());
        Layer.m_Tele = Layer.m_Speedup = Layer.m_Front = Layer.m_Switch = Layer.m_Tune = Layer.m_Data;
        Layer.m_Color.r = Layer.m_Color.g = Layer.m_Color.b = Layer.m_Color.a = 255;
        m_Data.push_back(Data); m_Layers.push_back(Layer); m_Group.m_NumLayers = static_cast<int>(m_Layers.size());
    }
    int GetDataSize(int Index) const override { return static_cast<int>(m_Data.at(static_cast<std::size_t>(Index)).size()); }
    void *GetData(int Index) override { return m_Data.at(static_cast<std::size_t>(Index)).data(); }
    void *GetDataSwapped(int Index) override { return GetData(Index); }
    const char *GetDataString(int) override { throw std::runtime_error("Owned physics map has no text"); }
    void UnloadData(int) override {}
    int NumData() const override { return static_cast<int>(m_Data.size()); }
    int GetItemSize(int Index) override { return Index==0 ? static_cast<int>(sizeof(m_Group)) : static_cast<int>(sizeof(CMapItemLayerTilemap)); }
    void *GetItem(int Index, int *Type=nullptr, int *Id=nullptr, CUuid * =nullptr) override
    {
        if(Index < 0 || Index >= NumItems()) throw std::runtime_error("Owned item bound");
        if(Type) *Type = Index==0 ? MAPITEMTYPE_GROUP : MAPITEMTYPE_LAYER;
        if(Id) *Id = Index==0 ? 0 : Index-1;
        if(Index==0) return &m_Group;
        return &m_Layers.at(static_cast<std::size_t>(Index-1));
    }
    void GetType(int Type, int *Start, int *Num) override
    {
        *Start=0; *Num=0;
        if(Type==MAPITEMTYPE_GROUP) { *Num=1; }
        else if(Type==MAPITEMTYPE_LAYER) { *Start=1; *Num=static_cast<int>(m_Layers.size()); }
    }
    int FindItemIndex(int Type, int Id) override
    {
        if(Type==MAPITEMTYPE_GROUP && Id==0) return 0;
        if(Type==MAPITEMTYPE_LAYER && Id>=0 && Id<static_cast<int>(m_Layers.size())) return Id+1;
        return -1;
    }
    void *FindItem(int Type, int Id) override { const int Index=FindItemIndex(Type,Id); return Index<0 ? nullptr : GetItem(Index); }
    int NumItems() const override { return static_cast<int>(m_Layers.size())+1; }
    bool Load(const char *, IStorage *, const char *, int) override { return false; }
    bool Load(IStorage *, const char *, int) override { return false; }
    void Unload() override {} // The owning C++ destructor owns every allocation.
    bool IsLoaded() const override { return true; }
    IOHANDLE File() const override { return nullptr; }
    const char *FullName() const override { return "owned-readonly-physics-snapshot"; }
    const char *BaseName() const override { return FullName(); }
    const char *Path() const override { return FullName(); }
    SHA256_DIGEST Sha256() const override { throw std::runtime_error("Owned physics map has no serialized identity"); }
    unsigned Crc() const override { throw std::runtime_error("Owned physics map has no serialized CRC"); }
    int Size() const override { throw std::runtime_error("Owned physics map has no serialized size"); }
};
