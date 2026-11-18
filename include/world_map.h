#pragma once

#include <vector>

class WorldMap {
public:
    WorldMap();

    int width() const;
    int height() const;
    bool contains(int x, int y) const;
    int tileAt(int x, int y) const;
    bool isWall(int x, int y) const;

private:
    int width_;
    int height_;
    std::vector<int> tiles_;
};
