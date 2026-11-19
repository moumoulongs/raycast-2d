#pragma once

// World coordinates, absolute foot elevation, heading and look angle (radians).
struct PlayerPose {
    double x{}, y{}, feet{}, yaw{}, pitch{};
};
