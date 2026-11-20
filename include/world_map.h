#pragma once
#include "vec2.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
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

// Compact 32-byte seamless world atmosphere zone (2 per 64B cache line).
struct MapZone {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float fogStart = 12.0f, fogEnd = 36.0f;
    std::uint8_t skyMode = 0; // 0=none, 1=starry, 2=eldritch
    std::uint8_t fogColor = 6;
    std::uint8_t reqFlag = 0; // 0 = always active, 1..63 = requires bit (reqFlag-1)
    std::uint8_t pad[5]{};
};
static_assert(sizeof(MapZone) == 32, "MapZone must be 32 bytes");

// Compact 20-byte story-driven smooth gate / drawbridge.
struct StoryGate {
    float closedFloor = 2.0f, openFloor = 0.0f, currentT = 0.0f, speed = 1.0f;
    std::uint8_t tileIdx = 0;
    std::uint8_t reqFlag = 0; // 1..63
    std::uint8_t pad[2]{};
};
static_assert(sizeof(StoryGate) == 20, "StoryGate must be 20 bytes");

// Compact 24-byte world trigger (interactable relic/NPC or walk-in zone).
struct MapTrigger {
    float x = 0, y = 0, radius = 1.8f;
    std::uint16_t eventIndex = 0;
    std::uint8_t kind = 0;       // 0 = interact (E), 1 = enter (auto)
    std::uint8_t reqFlag = 0;    // 0 = none, 1..63 = requires bit (reqFlag-1)
    std::uint8_t forbidFlag = 0; // 0 = none, 1..63 = hidden once bit (forbidFlag-1) is set
    std::uint8_t pad[7]{};
};
static_assert(sizeof(MapTrigger) == 24, "MapTrigger must be 24 bytes");

// Compact 16-byte conditional compass objective beacon.
struct MapBeacon {
    float x = 0, y = 0;
    std::uint8_t reqFlag = 0;
    std::uint8_t hideFlag = 0;
    std::uint8_t color = 9;
    std::uint8_t pad[5]{};
};
static_assert(sizeof(MapBeacon) == 16, "MapBeacon must be 16 bytes");

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
    WorldMap(std::string_view mapData,std::string_view atlasData,const std::filesystem::path& mapFile,const std::filesystem::path& textureFile);
    void animate(double seconds);
    void updateStoryWorld(double dt, std::uint64_t flags, double playerX, double playerY, int skyOverride = 0);
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
    vec2 beaconPosition() const { return hasBeacon_ ? beacon_ : vec2(gridWidth_ * 0.5, gridHeight_ * 0.5); }
    bool hasBeacon() const { return hasBeacon_; }
    const auto& zones() const { return zones_; }
    const auto& gates() const { return gates_; }
    const auto& triggers() const { return triggers_; }
    const std::string& triggerEventName(std::uint16_t idx) const { return triggerEvents_[idx]; }
    const std::string& storyFile() const { return storyFile_; }
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
    std::vector<MapZone> zones_;
    std::vector<StoryGate> gates_;
    std::vector<MapTrigger> triggers_;
    std::vector<std::string> triggerEvents_;
    std::vector<MapBeacon> beacons_;
    std::string storyFile_;
    SkyConfig sky_{}, baseSky_{};
    FogConfig fog_{}, baseFog_{};
    vec2 start_{}, direction_{}, beacon_{};
    bool hasBeacon_ = false;
    // Single contiguous texel arena across all textures (one allocation, maximal cache locality).
    std::vector<Texel> texelAtlas_;
    std::vector<WorldTexture> textures_; // slot 0 is empty sentinel; real textures are 1..255
    std::vector<std::uint8_t> texSlots_;
    std::vector<Tile> tiles_;
    std::vector<std::uint8_t> flatGrid_;
    int gridWidth_ = 0, gridHeight_ = 0;
    std::vector<WorldProp> props_;
};
