#include "camera.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kDefaultMoveSpeed = 4.0;
constexpr double kDefaultRotationSpeed = 2.2;

constexpr double kPlaneWidth = 0.66;

// First cell whose centre is on or after the projected edge.
int edge(double p, int extent) {
    return static_cast<int>(std::clamp(std::ceil(p - 0.5), 0.0, double(extent)));
}

} // namespace

Camera::Camera()
    : Camera(vec2(22.0, 12.0), vec2(-1.0, 0.0)) {}

Camera::Camera(vec2 position, vec2 forward)
    : position_(position),
      forward_(forward.normalized()),
      plane_(forward_.y * kPlaneWidth, -forward_.x * kPlaneWidth),
      moveSpeed_(kDefaultMoveSpeed),
      rotationSpeed_(kDefaultRotationSpeed) {}

bool Camera::canOccupy(const WorldMap& map, double x, double y) const {
    return map.contains(static_cast<int>(x), static_cast<int>(y)) &&
           !map.isWall(static_cast<int>(x), static_cast<int>(y)) &&
           map.at(int(x), int(y)).floor <= (initialized_ ? feet_ : map.at(int(position_.x), int(position_.y)).floor) + (grounded_ ? 0.26 : 0.0);
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

void Camera::advance(const WorldMap& map, double dt, int moveAxis, bool jump, int leftAxis) {
    auto floorHere = [&]{return map.at(int(position_.x),int(position_.y)).floor;};
    if (!initialized_) { feet_ = floorHere(); initialized_ = true; }
    if (jump && grounded_) { verticalSpeed_ = 4.8; grounded_ = false; }
    const double norm = (moveAxis && leftAxis) ? 0.7071067811865475 : 1.0;
    const double step = moveSpeed_ * dt * norm;
    const double x = position_.x + step * (moveAxis * forward_.x - leftAxis * forward_.y);
    const double y = position_.y + step * (moveAxis * forward_.y + leftAxis * forward_.x);
    if (canOccupy(map,x,position_.y)) position_.x=x;
    if (canOccupy(map,position_.x,y)) position_.y=y;
    const double ground = floorHere();
    if (grounded_ && feet_ - ground <= 0.26) feet_ = ground;
    else {
        grounded_ = false;
        feet_ += verticalSpeed_ * dt - 6.0 * dt * dt;
        verticalSpeed_ -= 12.0 * dt;
        if (feet_ <= ground) { feet_ = ground; verticalSpeed_ = 0; grounded_ = true; }
    }
    if (feet_ < -2.0) *this = Camera(map.spawnPosition(),map.spawnDirection());
}

void Camera::look(int dx, int dy) {
    if (dx) turn(-0.0025 * dx / rotationSpeed_);
    pitch_ = std::clamp(pitch_ - dy * 0.0025, -0.65, 0.65);
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

    if (screenWidth <= 0 || screenHeight <= 0 || forward_.magnitudeSquared() == 0) return;
    std::vector<double> wallDepth(size_t(screenWidth) * screenHeight, std::numeric_limits<double>::infinity());
    const double eye = (initialized_ ? feet_ : map.at(int(posX),int(posY)).floor) + 0.5;
    const double horizon = screenHeight * (0.5 + std::tan(pitch_));
    for (int x = 0; x < screenWidth; ++x) {
        const double cameraX = 2.0 * (x + 0.5) / screenWidth - 1.0;
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

        double entry = 0.00001;
        auto project = [&](double z,double d){return horizon+(eye-z)*screenHeight/d;};
        while (map.contains(mapX,mapY)) {
            const auto& tile = map.at(mapX,mapY);
            const double end = std::max(entry, std::min(sideDistX,sideDistY));
            // Project this tile's horizontal floor only over the ray interval inside it.
            const double first = project(tile.floor,entry), last = project(tile.floor,end);
            const auto& surface = map.texture(tile.surface);
            for(int y=edge(std::min(first,last),screenHeight); y<edge(std::max(first,last),screenHeight); ++y){
                if(tile.floor >= eye) break; // do not draw the underside
                const double d=(eye-tile.floor)*screenHeight/(y+0.5-horizon);
                auto& depth=wallDepth[y*screenWidth+x];
                if(d>0 && d<depth){depth=d; screen.drawCell(x,y,surface.sample(posX+d*rayDirX,posY+d*rayDirY),surface.color);}
            }
            if(sideDistX < sideDistY){side=0;mapX+=stepX;sideDistX+=deltaDistX;}
            else {side=1;mapY+=stepY;sideDistY+=deltaDistY;}
            if(!map.contains(mapX,mapY)) break;
            entry=end;
            const auto& next=map.at(mapX,mapY);
            const double top=next.floor+(next.wall?1.0:0.0);
            if(top>tile.floor){
                const auto& tex=map.texture(next.wall?next.wall:next.surface);
                const double u=side?posX+end*rayDirX:posY+end*rayDirY;
                for(int y=edge(project(top,end),screenHeight);y<edge(project(tile.floor,end),screenHeight);++y){
                    auto& depth=wallDepth[y*screenWidth+x];
                    if(end<depth){depth=end; const double z=eye-(y+0.5-horizon)*end/screenHeight;
                        screen.drawCell(x,y,tex.sample(u,top-z),tex.color);}
                }
            }
            if(next.wall) break;
        }
    }

    struct VisibleProp { const WorldProp* prop; double depth; double lateral; };
    std::vector<VisibleProp> visible;
    for (const auto& prop : map.props()) {
        const vec2 offset = vec2(prop.x, prop.y) - position_;
        const double depth = offset * forward_;
        if (depth > 0.01) visible.push_back({&prop, depth, offset * plane_ / (kPlaneWidth * kPlaneWidth)});
    }
    std::stable_sort(visible.begin(), visible.end(), [](const VisibleProp& a, const VisibleProp& b) {
        return a.depth > b.depth;
    });
    for (const auto& item : visible) {
        const auto& prop = *item.prop;
        const double scale = screenHeight / item.depth;
        const double height = scale * prop.height;
        const double floorY = horizon + scale * (eye-map.at(int(prop.x),int(prop.y)).floor);
        const double top = floorY - height;
        const double width = screenWidth * 0.5 * prop.width / (kPlaneWidth * item.depth);
        const double left = screenWidth * 0.5 * (1.0 + item.lateral / item.depth) - width * 0.5;
        const int bottom = edge(floorY, screenHeight), right = edge(left + width, screenWidth);
        for (int x = edge(left, screenWidth); x < right; ++x) {

            for (int y = edge(top, screenHeight); y < bottom; ++y) {
                if(item.depth >= wallDepth[y*screenWidth+x]) continue;
                const auto& texture = map.texture(prop.textureId);
                const char glyph = texture.sample( (x + 0.5 - left) / width, (y + 0.5 - top) / height);
                if (glyph != ' ') screen.drawCell(x, y, glyph, texture.color);
            }
        }
    }
}
