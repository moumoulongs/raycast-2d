#pragma once
#include "vec2.h"
#include <filesystem>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct WorldTexture { std::string pixels; std::uint16_t width, height; unsigned char color; float flowX = 0, flowY = 0; char sample(double u,double v) const; };
struct WorldProp { double x,y,width,height; int textureId; };
struct Swarm {double x,y,radius,height; int count;};
struct Tile { double floor; int wall, surface; };
class WorldMap {
public:
    WorldMap(const std::filesystem::path& mapFile,const std::filesystem::path& textureFile);
    WorldMap(std::istream& map,std::istream& atlas,const std::filesystem::path& mapFile,const std::filesystem::path& textureFile);
    void animate(double seconds);
    bool contains(int x,int y) const;
    const Tile& at(int x,int y) const { return legend_.at(grid_.at(y).at(x)); }
    bool isWall(int x,int y) const { return !contains(x,y) || at(x,y).wall != 0; }
    const WorldTexture& texture(int id) const { return textures_.at(id); }
    const auto& swarms() const {return swarms_;}
    const auto& props() const { return props_; }
    vec2 spawnPosition() const { return start_; }
    vec2 spawnDirection() const { return direction_; }
private:
    struct Lift {char symbol; double low, high, period, offset;};
    std::vector<Swarm> swarms_;
    std::vector<Lift> lifts_;
    vec2 start_{}, direction_{};
    std::map<int,WorldTexture> textures_;
    std::map<char,Tile> legend_;
    std::vector<std::string> grid_;
    std::vector<WorldProp> props_;
};
