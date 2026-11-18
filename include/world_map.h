#pragma once
#include "vec2.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct WorldTexture { unsigned char color; std::vector<std::string> rows; char sample(double u,double v) const; };
struct WorldProp { double x,y,width,height; int textureId; };
struct Tile { double floor; int wall, surface; };
class WorldMap {
public:
    WorldMap(const std::filesystem::path& mapFile,const std::filesystem::path& textureFile);
    WorldMap(std::istream& map,std::istream& atlas,const std::filesystem::path& mapFile,const std::filesystem::path& textureFile);
    bool contains(int x,int y) const;
    const Tile& at(int x,int y) const { return legend_.at(grid_.at(y).at(x)); }
    bool isWall(int x,int y) const { return !contains(x,y) || at(x,y).wall != 0; }
    const WorldTexture& texture(int id) const { return textures_.at(id); }
    const auto& props() const { return props_; }
    vec2 spawnPosition() const { return start_; }
    vec2 spawnDirection() const { return direction_; }
private:
    vec2 start_{}, direction_{};
    std::map<int,WorldTexture> textures_;
    std::map<char,Tile> legend_;
    std::vector<std::string> grid_;
    std::vector<WorldProp> props_;
};
