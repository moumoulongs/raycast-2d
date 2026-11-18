#include "camera.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kDefaultMoveSpeed = 4.0;
constexpr double kDefaultRotationSpeed = 2.2;

} // namespace

Camera::Camera()
    : Camera(vec2(22.0, 12.0), vec2(-1.0, 0.0)) {}

Camera::Camera(vec2 position, vec2 forward)
    : position_(position),
      forward_(forward.normalized()),
      plane_(0.0, 0.66),
      moveSpeed_(kDefaultMoveSpeed),
      rotationSpeed_(kDefaultRotationSpeed) {}

bool Camera::canOccupy(const WorldMap& map, double x, double y) const {
    return map.contains(static_cast<int>(x), static_cast<int>(y)) &&
           !map.isWall(static_cast<int>(x), static_cast<int>(y));
}

void Camera::move(const WorldMap& map, double seconds) {
    const double nextX = position_.x + forward_.x * moveSpeed_ * seconds;
    const double nextY = position_.y + forward_.y * moveSpeed_ * seconds;

    if (canOccupy(map, nextX, position_.y)) {
        position_.x = nextX;
    }
    if (canOccupy(map, position_.x, nextY)) {
        position_.y = nextY;
    }
}

void Camera::turn(double seconds) {
    const double cosine = std::cos(rotationSpeed_ * seconds);
    const double sine = std::sin(rotationSpeed_ * seconds);

    const double oldDirX = forward_.x;
    forward_.x = forward_.x * cosine - forward_.y * sine;
    forward_.y = oldDirX * sine + forward_.y * cosine;

    const double oldPlaneX = plane_.x;
    plane_.x = plane_.x * cosine - plane_.y * sine;
    plane_.y = oldPlaneX * sine + plane_.y * cosine;
}

void Camera::render(const WorldMap& map, Screen& screen) const {
    const double posX = position_.x;
    const double posY = position_.y;
    const double dirX = forward_.x;
    const double dirY = forward_.y;
    const int screenWidth = screen.width();
    const int screenHeight = screen.height();

    for (int x = 0; x < screenWidth; ++x) {
        const double cameraX = 2.0 * x / static_cast<double>(screenWidth) - 1.0;
        const double rayDirX = dirX + plane_.x * cameraX;
        const double rayDirY = dirY + plane_.y * cameraX;

        int mapX = static_cast<int>(posX);
        int mapY = static_cast<int>(posY);

        const double deltaDistX = (rayDirX == 0.0) ? std::numeric_limits<double>::max() : std::abs(1.0 / rayDirX);
        const double deltaDistY = (rayDirY == 0.0) ? std::numeric_limits<double>::max() : std::abs(1.0 / rayDirY);

        double sideDistX = 0.0;
        double sideDistY = 0.0;

        int stepX = 0;
        int stepY = 0;
        int side = 0;
        bool hit = false;

        if (rayDirX < 0.0) {
            stepX = -1;
            sideDistX = (posX - mapX) * deltaDistX;
        } else {
            stepX = 1;
            sideDistX = (mapX + 1.0 - posX) * deltaDistX;
        }

        if (rayDirY < 0.0) {
            stepY = -1;
            sideDistY = (posY - mapY) * deltaDistY;
        } else {
            stepY = 1;
            sideDistY = (mapY + 1.0 - posY) * deltaDistY;
        }

        while (!hit && map.contains(mapX, mapY)) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            if (!map.contains(mapX, mapY)) {
                break;
            }

            if (map.isWall(mapX, mapY)) {
                hit = true;
            }
        }

        if (!hit) {
            continue;
        }

        const double perpendicularDistance = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
        if (perpendicularDistance <= 0.0) {
            continue;
        }

        const int lineHeight = static_cast<int>(screenHeight / perpendicularDistance);
        const int drawStart = std::max(0, -lineHeight / 2 + screenHeight / 2);
        const int drawEnd = std::min(screenHeight - 1, lineHeight / 2 + screenHeight / 2);
        const std::uint8_t color = (side == 1) ? 98 : 255;

        screen.drawLine(x, drawStart, x, drawEnd, color);
    }
}
