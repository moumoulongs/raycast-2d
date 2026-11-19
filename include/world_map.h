#pragma once
#include "vec2.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

struct Texel { char glyph; unsigned char color; };
static_assert(sizeof(Texel) == 2, "Texel must remain 2 bytes for cache locality");

// Compact 32-byte POD texture descriptor (half cache line; zero per-texture heap pointers).
// All texel pixels across all textures live contiguously in WorldMap::texelAtlas_.
struct WorldTexture {
    std::uint32_t texelOffset = 0;
    std::uint32_t frameStride = 0;
    float flowX = 0.0f;
    float flowY = 0.0f;
    float fps = 0.0f;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t frames = 1;
    std::uint8_t color = 7;
    std::uint8_t flags = 0; // bit 0: hasFlow, bit 1: animated
};
static_assert(sizeof(WorldTexture) == 28, "WorldTexture must be exactly 28 bytes (zero padding)");

// Compact 32-byte prop descriptor (2 props per 64-byte cache line).
struct WorldProp {
    float x = 0, y = 0, width = 1, height = 1;
    float floatAmp = 0, floatSpeed = 0, baseOffset = 0;
    std::uint16_t textureId = 0;
    std::uint8_t textureSlot = 0;
    std::uint8_t pad = 0;
};
static_assert(sizeof(WorldProp) == 32, "WorldProp must be exactly 32 bytes");

struct Swarm { float x, y, radius, height; std::uint16_t count; std::uint16_t pad = 0; };
static_assert(sizeof(Swarm) == 20, "Swarm must be 20 bytes");

// Compact 32-byte particle emitter (2 emitters per 64-byte cache line).
struct Emitter {
    enum class Kind : std::uint8_t { Vortex, Rain, Spores, Geyser, Watchers, Lightning, Halo };
    float x = 0, y = 0, radius = 1, zMin = 0, zMax = 2, speed = 1;
    std::uint16_t count = 16;
    Kind kind = Kind::Vortex;
    std::uint8_t color1 = 10;
    std::uint8_t color2 = 13;
    std::uint8_t pad[3]{};
};
static_assert(sizeof(Emitter) == 32, "Emitter must be exactly 32 bytes");

struct SkyConfig { bool enabled = false; bool eldritch = false; };
struct FogConfig { bool enabled = false; std::uint8_t color = 8; float start = 12.0f, end = 28.0f; };

// Exact 16-byte Tile (4 tiles per 64-byte L1 cache line, zero wasted padding).
// wall/surface/side store direct 1-based texture slots (0 = none), eliminating secondary indirection!
struct Tile {
    double floor = 0.0;
    float wallHeight = 1.0f;
    std::uint8_t wall = 0;
    std::uint8_t surface = 0;
    std::uint8_t side = 0;
    std::uint8_t pad = 0;
};
static_assert(sizeof(Tile) == 16, "Tile must be exactly 16 bytes");

class WorldMap {
public:
    WorldMap(const std::filesystem::path& mapFile,const std::filesystem::path& textureFile);
    WorldMap(std::istream& map,std::istream& atlas,const std::filesystem::path& mapFile,const std::filesystem::path& textureFile);
    void animate(double seconds);
    bool contains(int x,int y) const {
        return static_cast<unsigned>(x) < static_cast<unsigned>(gridWidth_) &&
               static_cast<unsigned>(y) < static_cast<unsigned>(gridHeight_);
    }
    // O(1) flat 1-byte-per-cell lookup (entire 45x45 map = 2 KB in L1 cache).
    const Tile& at(int x,int y) const { return tiles_[flatGrid_[static_cast<size_t>(y) * gridWidth_ + x]]; }
    bool isWall(int x,int y) const { return !contains(x,y) || at(x,y).wall != 0; }

    // Direct 1-based slot access (zero secondary lookup in hot render loops).
    const WorldTexture& textureBySlot(std::uint8_t slot) const { return textures_[slot]; }
    const WorldTexture& texture(int id) const { return textures_[texSlots_[id]]; }
    const Texel* frameData(const WorldTexture& tex, double time) const {
        if (tex.frames <= 1 || tex.fps <= 0.0f) return texelAtlas_.data() + tex.texelOffset;
        const long long raw = static_cast<long long>(std::floor(time * tex.fps)) % tex.frames;
        const size_t f = static_cast<size_t>(raw < 0 ? raw + tex.frames : raw);
        return texelAtlas_.data() + tex.texelOffset + f * tex.frameStride;
    }

    int gridWidth() const { return gridWidth_; }
    int gridHeight() const { return gridHeight_; }
    const auto& swarms() const { return swarms_; }
    const auto& emitters() const { return emitters_; }
    const auto& props() const { return props_; }
    const SkyConfig& sky() const { return sky_; }
    const FogConfig& fog() const { return fog_; }
    vec2 spawnPosition() const { return start_; }
    vec2 spawnDirection() const { return direction_; }
private:
    struct Lift { float low, high, period, offset; std::uint8_t tileIdx; };
    struct Wave { float low, high, period, offset; std::uint8_t tileIdx; };
    struct Monolith { float lowH, highH, period, offset; std::uint8_t tileIdx; };
    struct Morph { float period, offset; std::uint8_t tileIdx, wall1, surf1, side1, wall2, surf2, side2; };

    std::vector<Swarm> swarms_;
    std::vector<Emitter> emitters_;
    std::vector<Lift> lifts_;
    std::vector<Wave> waves_;
    std::vector<Monolith> monoliths_;
    std::vector<Morph> morphs_;
    SkyConfig sky_{};
    FogConfig fog_{};
    vec2 start_{}, direction_{};
    // Single contiguous texel arena across all textures (one allocation, maximal cache locality).
    std::vector<Texel> texelAtlas_;
    std::vector<WorldTexture> textures_; // slot 0 is empty sentinel; real textures are 1..255
    std::vector<std::uint8_t> texSlots_;
    std::vector<Tile> tiles_;
    std::vector<std::uint8_t> flatGrid_;
    int gridWidth_ = 0, gridHeight_ = 0;
    std::vector<WorldProp> props_;
};
