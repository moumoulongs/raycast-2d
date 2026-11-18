#pragma once

#include "screen.h"
#include "vec2.h"
#include "world_map.h"

class Camera {
public:
    Camera();
    Camera(vec2 position, vec2 forward);

    void move(const WorldMap& map, double seconds);
    void turn(double seconds);
    void render(const WorldMap& map, Screen& screen) const;

private:
    bool canOccupy(const WorldMap& map, double x, double y) const;

    vec2 position_;
    vec2 forward_;
    vec2 plane_;
    double moveSpeed_;
    double rotationSpeed_;
};
