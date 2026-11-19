#pragma once

#include "player_pose.h"
#include "screen.h"
#include "vec2.h"
#include "world_map.h"

class Camera {
public:
    Camera();
    Camera(vec2 position, vec2 forward);

    void move(const WorldMap& map, double seconds);
    void advance(const WorldMap& map, double dt, int moveAxis, bool jump, int leftAxis = 0);
    void look(int dx, int dy);
    void turn(double seconds);
    PlayerPose pose() const;
    void applyPose(const PlayerPose& pose);
    void render(const WorldMap& map, Screen& screen, const PlayerPose* other = nullptr, double animationTime = 0) const;

private:
    bool canOccupy(const WorldMap& map, double x, double y) const;

    // Column-major scratch (4-byte float for 2x cache density): DDA writes vertical spans consecutively.
    mutable std::vector<float> depthBuffer_;
    mutable std::vector<double> invRowDist_;
    struct VisibleProp { const WorldProp* prop; double depth; double lateral; };
    mutable std::vector<VisibleProp> visibleProps_;

    double pitch_ = 0;
    double feet_ = 0, verticalSpeed_ = 0;
    bool initialized_ = false, grounded_ = true;
    vec2 position_;
    vec2 forward_;
    vec2 plane_;
    double moveSpeed_;
    double rotationSpeed_;
};
