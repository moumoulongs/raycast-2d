#include "camera.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <limits>
#include <mutex>
#include <thread>

namespace {

constexpr double kDefaultMoveSpeed = 4.0;
constexpr double kDefaultRotationSpeed = 2.2;

constexpr double kPlaneWidth = 0.66;
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

// First cell whose centre is on or after the projected edge.
int edge(double p, int extent) {
    return static_cast<int>(std::clamp(std::ceil(p - 0.5), 0.0, double(extent)));
}

bool gridCoordinate(double value) {
    return value >= 0 && value < std::numeric_limits<int>::max();
}

bool validPose(const PlayerPose& p) {
    return gridCoordinate(p.x) && gridCoordinate(p.y) && std::isfinite(p.feet) &&
           std::abs(p.feet) < std::numeric_limits<int>::max() &&
           std::isfinite(p.yaw) && std::isfinite(p.pitch);
}

// A transparent, camera-facing figure with a compass-relative face/visor.
char playerGlyph(double u, double v, char facing) {
    const double side = std::abs(u - 0.5);
    if (v < 0.27) return side < 0.28 ? facing : ' ';
    if (v < 0.64) return side < 0.33 ? 'H' : '|';
    return side > 0.08 && side < 0.32 ? '|' : ' ';
}

std::uint32_t hash2D(int x, int y) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

double wrapAngle(double a) {
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

Texel applyFog(Texel cell, double dist, const FogConfig& fog, double stormFlash) {
    if (!fog.enabled || cell.glyph == ' ') return cell;
    if (dist <= fog.start) return cell;
    const double span = std::max<double>(0.01, fog.end - fog.start);
    const double t = std::clamp((dist - fog.start) / span, 0.0, 1.0);
    // Brief cosmic lightning illuminates the fogged horizon!
    const double effT = std::max(0.0, t - stormFlash * 0.35);
    if (effT < 0.45) {
        if (cell.color >= 8 && cell.color < 15) cell.color = static_cast<unsigned char>(cell.color - 8);
        return cell;
    }
    if (effT < 0.82) {
        if (cell.color >= 8) cell.color = static_cast<unsigned char>(cell.color == 15 ? 7 : cell.color - 8);
        if (cell.color == 7) cell.color = 8;
        if (effT > 0.68 && (cell.glyph == '#' || cell.glyph == '=' || cell.glyph == '@' || cell.glyph == 'M')) {
            cell.glyph = ':';
        }
        return cell;
    }
    cell.color = fog.color;
    if (effT > 0.94) cell.glyph = '.';
    else if (cell.glyph != '.' && cell.glyph != ',' && cell.glyph != '~') cell.glyph = ':';
    return cell;
}

inline size_t wrapTex(double p, size_t n) {
    const long long ip = static_cast<long long>(p);
    double frac = p - static_cast<double>(ip);
    if (frac < 0.0) frac += 1.0;
    const auto i = static_cast<size_t>(frac * n);
    return i < n ? i : size_t{0};
}

// 256-entry L1-resident sine table for zero-trig inner-loop sky twinkle & shimmer.
struct SinLut {
    std::array<float, 256> v{};
    SinLut() {
        for (int i = 0; i < 256; ++i) {
            v[static_cast<size_t>(i)] = static_cast<float>(std::sin(i * (kTwoPi / 256.0)));
        }
    }
    inline float get(double radians) const {
        const int idx = static_cast<int>(radians * (256.0 / kTwoPi)) & 255;
        return v[static_cast<size_t>(idx)];
    }
};
const SinLut& sinLut() {
    static const SinLut lut;
    return lut;
}

// Persistent column-stripe thread pool for large high-res viewports (zero per-frame thread creation).
class ColumnPool {
public:
    ColumnPool() {
        const unsigned hw = std::thread::hardware_concurrency();
        const unsigned workers = hw > 1 ? std::min(hw - 1, 7u) : 0u;
        threads_.reserve(workers);
        for (unsigned i = 0; i < workers; ++i) {
            threads_.emplace_back([this, i] { workerLoop(i); });
        }
    }
    ~ColumnPool() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            stop_ = true;
        }
        cvStart_.notify_all();
        for (auto& t : threads_) t.join();
    }

    template <typename Fn>
    void parallelColumns(int totalCols, Fn&& fn) {
        if (threads_.empty() || totalCols < 256) {
            fn(0, totalCols);
            return;
        }
        const int parts = static_cast<int>(threads_.size()) + 1;
        // Align slice boundaries to 32 columns (64 bytes = 1 exact cache line) to eliminate false sharing.
        const int slice = ((totalCols / parts + 31) / 32) * 32;
        if (slice >= totalCols) {
            fn(0, totalCols);
            return;
        }
        std::function<void(int, int)> task = fn;
        {
            std::lock_guard<std::mutex> lk(mu_);
            task_ = &task;
            totalCols_ = totalCols;
            slice_ = slice;
            remaining_ = static_cast<int>(threads_.size());
            ++generation_;
        }
        cvStart_.notify_all();
        // Main thread executes stripe 0 while workers execute stripes 1..N.
        fn(0, std::min(slice, totalCols));
        std::unique_lock<std::mutex> lk(mu_);
        cvDone_.wait(lk, [&] { return remaining_ == 0; });
        task_ = nullptr;
    }

private:
    void workerLoop(unsigned index) {
        std::uint64_t seen = 0;
        for (;;) {
            std::function<void(int, int)>* task = nullptr;
            int x0 = 0, x1 = 0;
            {
                std::unique_lock<std::mutex> lk(mu_);
                cvStart_.wait(lk, [&] { return stop_ || generation_ != seen; });
                if (stop_) return;
                seen = generation_;
                task = task_;
                x0 = static_cast<int>(index + 1) * slice_;
                x1 = (index + 1 == threads_.size()) ? totalCols_ : std::min(totalCols_, x0 + slice_);
            }
            if (task && x0 < x1) (*task)(x0, x1);
            {
                std::lock_guard<std::mutex> lk(mu_);
                if (--remaining_ == 0) cvDone_.notify_one();
            }
        }
    }

    std::vector<std::thread> threads_;
    std::mutex mu_;
    std::condition_variable cvStart_, cvDone_;
    std::function<void(int, int)>* task_ = nullptr;
    int totalCols_ = 0, slice_ = 0, remaining_ = 0;
    std::uint64_t generation_ = 0;
    bool stop_ = false;
};

ColumnPool& columnPool() {
    static ColumnPool pool;
    return pool;
}

// Fast rational atan approximation for positive x (error < 0.004 rad).
inline double fastAtanPos(double x) {
    if (x <= 1.0) return x / (1.0 + 0.28 * x * x);
    return kPi * 0.5 - x / (x * x + 0.28);
}

struct SkyFrame {
    double eyeOpen = 0.0;
    bool meteorActive = false;
    double meteorHeadAz = 0.0;
    double meteorHeadAlt = 0.0;
};

SkyFrame prepareSkyFrame(double time, bool eldritch) {
    SkyFrame f{};
    f.eyeOpen = eldritch ? std::max(0.0, std::sin(time * 0.55) - 0.20) / 0.80 : 0.0;
    const double mPeriod = 5.5;
    const double mPhase = std::fmod(std::max(0.0, time), mPeriod);
    if (mPhase < 0.55) {
        const int cycle = static_cast<int>(std::max(0.0, time) / mPeriod);
        const auto mh = hash2D(cycle * 17 + 3, 911);
        const double startAz = -kPi + (mh % 1000) * (kTwoPi / 1000.0);
        const double startAlt = 0.28 + ((mh >> 10) % 220) * 0.001;
        const double prog = mPhase / 0.55;
        f.meteorHeadAz = wrapAngle(startAz + prog * 0.55);
        f.meteorHeadAlt = startAlt - prog * 0.18;
        f.meteorActive = true;
    }
    return f;
}

struct SkyColumn {
    double skyAz;
    double dAzSun;
    double dAzMoon1;
    double dAzMoon2;
    double riftCenter;
    double stormLimit;
    int stormCellX;
    double eyeOpen;
    bool sunNear;
    bool moon1Near;
    bool moon2Near;
    bool meteorActive;
    double meteorDAz;
    double meteorHeadAlt;
};

SkyColumn prepareSkyColumn(const SkyFrame& sf, double azimuth, double time, bool eldritch) {
    SkyColumn c{};
    c.skyAz = azimuth + time * 0.004;
    c.dAzSun = wrapAngle(azimuth + kPi * 0.5);
    c.dAzMoon1 = wrapAngle(azimuth - 0.85);
    c.dAzMoon2 = wrapAngle(azimuth - 2.35);
    c.sunNear = eldritch && std::abs(c.dAzSun) < 0.16;
    c.moon1Near = std::abs(c.dAzMoon1) < 0.05;
    c.moon2Near = eldritch && std::abs(c.dAzMoon2) < 0.036;
    c.riftCenter = 0.25 + 0.15 * std::sin(c.skyAz + 0.55) + 0.04 * std::sin(c.skyAz * 3.0 - time * 0.12);
    c.eyeOpen = sf.eyeOpen;
    if (eldritch) {
        const double drift = azimuth * 5.0 + time * 0.14;
        c.stormLimit = 0.062 + std::sin(drift) * 0.020 + std::sin(drift * 2.7 - time * 0.25) * 0.011;
        c.stormCellX = static_cast<int>(std::floor((azimuth + kPi) * 42.0 + time * 1.8));
    }
    if (sf.meteorActive) {
        c.meteorDAz = wrapAngle(azimuth - sf.meteorHeadAz);
        c.meteorHeadAlt = sf.meteorHeadAlt;
        c.meteorActive = std::abs(c.meteorDAz) < 0.13;
    }
    return c;
}

inline Texel sampleStarrySky(const SkyColumn& col, double altitude, double time, double stormFlash, bool eldritch) {
    // 1. Colossal Eclipsed Black Sun / Eye of Azathoth (column-culled!)
    if (col.sunNear) {
        const double dAlt = altitude - 0.30;
        if (std::abs(dAlt) < 0.16) {
            const double dist = std::hypot(col.dAzSun, dAlt);
            const double rDisc = 0.068;
            if (dist < rDisc * 0.92) {
                if (col.eyeOpen > 0.05 && std::abs(dAlt) < 0.036 * col.eyeOpen && std::abs(col.dAzSun) < 0.052) {
                    if (std::abs(col.dAzSun) < 0.010) return {'|', static_cast<unsigned char>(stormFlash > 0.3 ? 11 : 9)};
                    if (std::abs(col.dAzSun) < 0.024) return {'#', 9};
                    return {'-', 1};
                }
                return {' ', 0};
            }
            if (dist < 0.155) {
                const double ang = std::atan2(dAlt, col.dAzSun);
                if (dist < rDisc + 0.016) {
                    const bool crimson = std::sin(ang * 3.0 - time * 1.8) > 0.45 || stormFlash > 0.35;
                    return {'@', static_cast<unsigned char>(crimson ? 9 : 10)};
                }
                const double flare = 0.022 * std::sin(ang * 7.0 + time * 1.4) + 0.012 * std::cos(ang * 13.0 - time * 2.1);
                if (dist < rDisc + 0.050 + flare) {
                    const double rim = (dist - rDisc) / std::max(0.01, 0.050 + flare);
                    const char g = rim < 0.45 ? '*' : (rim < 0.75 ? '~' : '.');
                    const unsigned char c = rim < 0.38 ? (stormFlash > 0.3 ? 11 : 10) : (rim < 0.7 ? 2 : 5);
                    return {g, c};
                }
            }
        }
    }

    // 2. Pale Crescent Moon (column-culled!)
    if (col.moon1Near) {
        const double dAlt = altitude - 0.33;
        const double distSq = col.dAzMoon1 * col.dAzMoon1 + dAlt * dAlt;
        if (distSq < 0.044 * 0.044) {
            const double sdx = col.dAzMoon1 - 0.018, sdy = dAlt - 0.008;
            if (sdx * sdx + sdy * sdy < 0.036 * 0.036) return {'.', 8};
            return {distSq < 0.026 * 0.026 ? 'O' : '(', static_cast<unsigned char>(distSq < 0.02 * 0.02 ? 15 : 14)};
        }
    }

    // 3. Secondary Blood-Violet Ghost Moon (column-culled!)
    if (col.moon2Near) {
        const double dAlt = altitude - 0.29;
        const double distSq = col.dAzMoon2 * col.dAzMoon2 + dAlt * dAlt;
        if (distSq < 0.032 * 0.032) {
            return {distSq < 0.018 * 0.018 ? 'o' : ':', static_cast<unsigned char>(distSq < 0.018 * 0.018 ? 13 : 5)};
        }
    }

    // 4. Horizon storm cloud mist + distant sheet lightning
    if (eldritch && altitude < col.stormLimit) {
        const int cy = static_cast<int>(std::floor(altitude * 90.0));
        const auto h = hash2D(col.stormCellX, cy);
        if ((h & 3u) == 0u) {
            const char g = (h & 4u) ? '~' : (altitude < 0.032 ? '=' : '-');
            const unsigned char c = stormFlash > 0.45 ? ((h & 8u) ? 14 : 13) : ((h & 8u) ? 6 : 8);
            return {g, c};
        }
    }

    // 5. Shooting meteor (column-culled!)
    if (col.meteorActive) {
        const double dAlt = altitude - col.meteorHeadAlt;
        const double proj = col.meteorDAz * (-0.9) + dAlt * 0.43;
        const double perp = std::abs(col.meteorDAz * 0.43 + dAlt * 0.9);
        if (proj >= 0.0 && proj < 0.11 && perp < 0.009) {
            return {proj < 0.022 ? '*' : (proj < 0.06 ? '\\' : '.'), static_cast<unsigned char>(proj < 0.03 ? 15 : 14)};
        }
    }

    // 6. Diagonal Milky Way / Astral Nebula Rift + Rich Multi-Magnitude 3D Starfield
    // Fast cosine approximation on [0, pi/2]: cos(a) ~= 1 - 0.45 * a * a
    const double cosScale = std::max(0.25, 1.0 - 0.45 * altitude * altitude);
    const int azCell = static_cast<int>((col.skyAz + kPi) * 92.0 * cosScale);
    const int altCell = static_cast<int>(altitude * 92.0);
    const std::uint32_t h = hash2D(azCell, altCell);

    const double riftDist = std::abs(altitude - col.riftCenter);
    const bool inRift = riftDist < 0.14;

    const std::uint32_t starMask = inRift ? 7u : 15u;
    if ((h & starMask) == 0u) {
        const float twinkle = sinLut().get(time * (1.3 + ((h >> 16) & 7u) * 0.42) + static_cast<double>(h & 127u));
        const unsigned mag = (h >> 5) & 7u;
        if (mag == 7u && twinkle > -0.35f) {
            static constexpr unsigned char kBright[] = {15, 14, 10, 13, 11, 14, 10, 15};
            return {twinkle > 0.40f ? '*' : '+', kBright[(h >> 8) & 7u]};
        }
        if (mag >= 4u) {
            static constexpr unsigned char kMid[] = {15, 7, 14, 13, 10, 12, 14, 7};
            return {twinkle > 0.25f ? '+' : (mag == 6u ? '*' : '.'), kMid[(h >> 8) & 7u]};
        }
        if (twinkle > -0.65f) {
            static constexpr unsigned char kDim[] = {8, 4, 6, 5, 7, 4, 6, 8};
            return {(h & 256u) ? '\'' : '.', kDim[(h >> 9) & 7u]};
        }
    }

    if (inRift && (h & 3u) == 2u && riftDist < 0.115) {
        const float shimmer = sinLut().get(time * 0.7 + col.skyAz * 4.0 + altitude * 8.0);
        if (shimmer > -0.15f) {
            const unsigned char c = (h & 32u) ? (eldritch ? 5 : 4) : 6;
            return {(h & 16u) ? ':' : '.', c};
        }
    }

    return {' ', 0};
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

PlayerPose Camera::pose() const {
    return {position_.x, position_.y, feet_, std::atan2(forward_.y, forward_.x), pitch_};
}

void Camera::applyPose(const PlayerPose& p) {
    if (!validPose(p)) return;
    position_ = vec2(p.x, p.y);
    forward_ = vec2(std::cos(p.yaw), std::sin(p.yaw));
    plane_ = vec2(forward_.y * kPlaneWidth, -forward_.x * kPlaneWidth);
    feet_ = p.feet;
    pitch_ = std::clamp(p.pitch, -0.65, 0.65);
    initialized_ = true;
    grounded_ = true;
    verticalSpeed_ = 0;
}

void Camera::render(const WorldMap& map, Screen& screen, const PlayerPose* other, double animationTime) const {
    const double posX = position_.x;
    const double posY = position_.y;
    const double dirX = forward_.x;
    const double dirY = forward_.y;
    const int screenWidth = screen.width();
    const int screenHeight = screen.height();

    if (screenWidth <= 0 || screenHeight <= 0 || forward_.magnitudeSquared() == 0) return;
    auto& wallDepth = depthBuffer_;
    const size_t totalDepth = static_cast<size_t>(screenWidth) * screenHeight;
    if (wallDepth.size() != totalDepth) wallDepth.resize(totalDepth);
    const double eye = (initialized_ ? feet_ : map.at(int(posX),int(posY)).floor) + 0.5;
    const double horizon = screenHeight * (0.5 + std::tan(pitch_));
    const double invScreenHeight = 1.0 / screenHeight;

    // Precompute row inverse distances once per frame (replaces W*H divisions with H divisions).
    invRowDist_.resize(static_cast<size_t>(screenHeight));
    for (int y = 0; y < screenHeight; ++y) {
        const double denom = (y + 0.5) - horizon;
        invRowDist_[static_cast<size_t>(y)] = (std::abs(denom) > 1e-9) ? (screenHeight / denom) : 0.0;
    }

    const auto& fog = map.fog();
    const bool useFog = fog.enabled;
    const bool hasSky = map.sky().enabled;
    const bool eldritchSky = map.sky().eldritch;
    double stormFlash = 0.0;
    if (eldritchSky) {
        const double beat = std::fmod(std::max(0.0, animationTime), 11.0);
        if (beat < 0.22) stormFlash = 1.0 - beat / 0.22;
        else if (beat > 0.34 && beat < 0.50) stormFlash = 0.75 * (1.0 - (beat - 0.34) / 0.16);
    }
    const int skyLimit = hasSky ? std::clamp(static_cast<int>(std::ceil(horizon)), 0, screenHeight) : 0;
    const SkyFrame skyFrame = hasSky ? prepareSkyFrame(animationTime, eldritchSky) : SkyFrame{};
    constexpr float kInf = std::numeric_limits<float>::infinity();

    columnPool().parallelColumns(screenWidth, [&](int xBegin, int xEnd) {
    for (int x = xBegin; x < xEnd; ++x) {
        // Initialize this column's depth buffer right before DDA so it stays 100% hot in L1D cache!
        float* const depthCol = wallDepth.data() + static_cast<size_t>(x) * screenHeight;
        std::fill_n(depthCol, screenHeight, kInf);

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

        int skyStop = skyLimit;
        double entry = 0.00001;
        auto project = [&](double z,double d){return horizon+(eye-z)*screenHeight/d;};
        while (map.contains(mapX,mapY)) {
            const auto& tile = map.at(mapX,mapY);
            const double end = std::max(entry, std::min(sideDistX,sideDistY));
            // Project this tile's horizontal floor only over the ray interval inside it.
            if (tile.floor < eye) {
                const double first = project(tile.floor,entry), last = project(tile.floor,end);
                const int y0 = edge(std::min(first,last),screenHeight);
                const int y1 = edge(std::max(first,last),screenHeight);
                if (y0 < y1) {
                    const auto& surface = map.textureBySlot(tile.surface);
                    const Texel* const surfFrame = map.frameData(surface, animationTime);
                    const size_t sw = surface.width, sh = surface.height;
                    const double originX = posX - animationTime * surface.flowX;
                    const double originY = posY - animationTime * surface.flowY;
                    const double eyeMinusFloor = eye - tile.floor;
                    for (int y = y0; y < y1; ++y) {
                        const double d = eyeMinusFloor * invRowDist_[static_cast<size_t>(y)];
                        const float df = static_cast<float>(d);
                        if (d > 0.0 && df < depthCol[y]) {
                            depthCol[y] = df;
                            const size_t tx = wrapTex(originX + d * rayDirX, sw);
                            const size_t ty = wrapTex(originY + d * rayDirY, sh);
                            Texel texel = surfFrame[ty * sw + tx];
                            if (useFog) texel = applyFog(texel, d, fog, stormFlash);
                            screen.drawCellUnchecked(x, y, texel.glyph, texel.color);
                        }
                    }
                }
            }
            if(sideDistX < sideDistY){side=0;mapX+=stepX;sideDistX+=deltaDistX;}
            else {side=1;mapY+=stepY;sideDistY+=deltaDistY;}
            if(!map.contains(mapX,mapY)) break;
            entry=end;
            const auto& next=map.at(mapX,mapY);
            const double top=next.floor+(next.wall?static_cast<double>(next.wallHeight):0.0);
            if(top>tile.floor){
                const int y0 = edge(project(top,end),screenHeight);
                const int y1 = edge(project(tile.floor,end),screenHeight);
                if (y0 < y1) {
                    if (y1 >= skyStop && y0 < skyStop) skyStop = y0;
                    const std::uint8_t texSlot = next.wall ? next.wall : (next.side ? next.side : next.surface);
                    const auto& tex=map.textureBySlot(texSlot);
                    const Texel* const wallFrame = map.frameData(tex, animationTime);
                    const size_t tw = tex.width, th = tex.height;
                    const double u=(side?posX+end*rayDirX:posY+end*rayDirY) - animationTime * tex.flowX;
                    const size_t tx = wrapTex(u, tw); // invariant across the entire vertical wall span!
                    const double vStep = end * invScreenHeight;
                    double v = (top - eye) + (y0 + 0.5 - horizon) * vStep - animationTime * tex.flowY;
                    const float endf = static_cast<float>(end);
                    for (int y = y0; y < y1; ++y, v += vStep) {
                        if (endf < depthCol[y]) {
                            depthCol[y] = endf;
                            Texel texel = wallFrame[wrapTex(v, th) * tw + tx];
                            if (useFog) texel = applyFog(texel, end, fog, stormFlash);
                            screen.drawCellUnchecked(x, y, texel.glyph, texel.color);
                        }
                    }
                }
            }
            if(next.wall) break;
        }

        if (hasSky && skyStop > 0) {
            const double azimuth = std::atan2(rayDirY, rayDirX);
            const SkyColumn skyCol = prepareSkyColumn(skyFrame, azimuth, animationTime, eldritchSky);
            const double invDenom = 1.0 / (screenHeight * std::hypot(rayDirX, rayDirY));
            for (int y = 0; y < skyStop; ++y) {
                if (!std::isinf(depthCol[y])) continue;
                const double tanAlt = (horizon - (y + 0.5)) * invDenom;
                if (tanAlt <= 0.0) continue;
                const double altitude = fastAtanPos(tanAlt);
                const auto skyCell = sampleStarrySky(skyCol, altitude, animationTime, stormFlash, eldritchSky);
                if (skyCell.glyph != ' ') screen.drawCellUnchecked(x, y, skyCell.glyph, skyCell.color);
            }
        }
    }
    });

    const bool trackPropDepth = other || !map.swarms().empty() || !map.emitters().empty();
    visibleProps_.clear();
    for (const auto& prop : map.props()) {
        const vec2 offset = vec2(prop.x, prop.y) - position_;
        const double depth = offset * forward_;
        if (depth > 0.01) visibleProps_.push_back({&prop, depth, offset * plane_ / (kPlaneWidth * kPlaneWidth)});
    }
    std::stable_sort(visibleProps_.begin(), visibleProps_.end(), [](const VisibleProp& a, const VisibleProp& b) {
        return a.depth > b.depth;
    });
    for (const auto& item : visibleProps_) {
        const auto& prop = *item.prop;
        const double scale = screenHeight / item.depth;
        const double height = scale * prop.height;
        const double bob = prop.floatAmp != 0.0f ? prop.floatAmp * std::sin(animationTime * prop.floatSpeed + prop.x * 1.7 + prop.y * 2.3) : 0.0;
        const double baseZ = map.at(int(prop.x),int(prop.y)).floor + prop.baseOffset + bob;
        const double floorY = horizon + scale * (eye - baseZ);
        const double top = floorY - height;
        const double width = screenWidth * 0.5 * prop.width / (kPlaneWidth * item.depth);
        const double left = screenWidth * 0.5 * (1.0 + item.lateral / item.depth) - width * 0.5;
        const int x0 = edge(left, screenWidth), right = edge(left + width, screenWidth);
        const int y0 = edge(top, screenHeight), bottom = edge(floorY, screenHeight);
        if (x0 >= right || y0 >= bottom) continue;
        const auto& texture = map.textureBySlot(prop.textureSlot);
        const Texel* const propFrame = map.frameData(texture, animationTime);
        const size_t pw = texture.width, ph = texture.height;
        const double invW = 1.0 / width, invH = 1.0 / height;
        const bool hasFlow = (texture.flags & 1u) != 0u;
        const double uShift = hasFlow ? -animationTime * texture.flowX : 0.0;
        const double vShift = hasFlow ? -animationTime * texture.flowY : 0.0;
        const float itemDepthF = static_cast<float>(item.depth);
        for (int x = x0; x < right; ++x) {
            float* const depthCol = wallDepth.data() + static_cast<size_t>(x) * screenHeight;
            const double u = (x + 0.5 - left) * invW + uShift;
            const size_t tx = hasFlow ? wrapTex(u, pw) : std::min(pw - 1, static_cast<size_t>(std::max(0.0, u) * pw));
            double v = (y0 + 0.5 - top) * invH + vShift;
            for (int y = y0; y < bottom; ++y, v += invH) {
                if (itemDepthF >= depthCol[y]) continue;
                const size_t ty = hasFlow ? wrapTex(v, ph) : std::min(ph - 1, static_cast<size_t>(std::max(0.0, v) * ph));
                Texel texel = propFrame[ty * pw + tx];
                if (texel.glyph != ' ') {
                    if (useFog) texel = applyFog(texel, item.depth, fog, stormFlash);
                    screen.drawCellUnchecked(x, y, texel.glyph, texel.color);
                    if (trackPropDepth) depthCol[y] = itemDepthF;
                }
            }
        }
    }
    // Analytic spirals: late joiners need only the host clock, not particle packets.
    for (const auto& swarm : map.swarms()) {
        for(int i=0;i<swarm.count;++i){
            const double rise=std::fmod(animationTime*(0.13+0.012*(i%5))+i*0.618034,1.0);
            const double angle=i*2.399963+rise*4.0+animationTime*0.2;
            const double radius=swarm.radius*(0.15+0.85*rise);
            const vec2 v(swarm.x+radius*std::cos(angle)-posX,swarm.y+radius*std::sin(angle)-posY);
            const double d=v*forward_; if(d<0.08) continue;
            const double sx=screenWidth*.5*(1+(v*plane_)/(kPlaneWidth*kPlaneWidth*d));
            const double sy=horizon+(eye-rise*swarm.height)*screenHeight/d;
            if(sx<0||sx>=screenWidth||sy<0||sy>=screenHeight) continue;
            int x=int(sx),y=int(sy); auto& depth=wallDepth[size_t(x)*screenHeight+y];
            const float df = static_cast<float>(d);
            if(df<depth){depth=df;screen.drawCellUnchecked(x,y,rise>.75?'*':rise>.35?'+':'.',i%5?10:13);}
        }
    }
    // Rich eldritch particle emitters (100% analytic from animationTime).
    const double halfW = screenWidth * 0.5;
    constexpr double kInvPlaneSq = 1.0 / (kPlaneWidth * kPlaneWidth);
    auto plotParticle = [&](double wx, double wy, double wz, char glyph, unsigned char color, bool writeDepth) {
        const vec2 v(wx - posX, wy - posY);
        const double d = v * forward_;
        if (d < 0.08) return;
        const double invD = 1.0 / d;
        const double sx = halfW * (1.0 + (v * plane_) * kInvPlaneSq * invD);
        const double sy = horizon + (eye - wz) * screenHeight * invD;
        if (sx < 0 || sx >= screenWidth || sy < 0 || sy >= screenHeight) return;
        const int px = static_cast<int>(sx), py = static_cast<int>(sy);
        auto& depth = wallDepth[size_t(px) * screenHeight + py];
        const float df = static_cast<float>(d);
        if (df < depth) {
            if (writeDepth) depth = df;
            screen.drawCellUnchecked(px, py, glyph, color);
        }
    };
    const double tSafe = std::max(0.0, animationTime);
    for (size_t ei = 0; ei < map.emitters().size(); ++ei) {
        const auto& e = map.emitters()[ei];
        // Frustum back-plane sphere cull: skip entire emitter behind the camera in O(1).
        const double centerDepth = (e.x - posX) * dirX + (e.y - posY) * dirY;
        if (centerDepth < -static_cast<double>(e.radius) - 0.5) continue;
        const double zSpan = std::max(0.01, static_cast<double>(e.zMax - e.zMin));
        switch (e.kind) {
        case Emitter::Kind::Vortex: {
            for (int i = 0; i < e.count; ++i) {
                const double rise = std::fmod(tSafe * (0.14 + 0.015 * (i % 5)) * e.speed + i * 0.618034, 1.0);
                const double angle = i * 2.399963 + rise * 7.5 + tSafe * (0.9 * e.speed);
                const double rad = e.radius * (0.18 + 0.82 * std::pow(rise, 0.75));
                const double wx = e.x + rad * std::cos(angle);
                const double wy = e.y + rad * std::sin(angle);
                const double wz = e.zMin + rise * zSpan;
                const char g = rise > 0.82 ? '@' : rise > 0.58 ? '*' : rise > 0.30 ? '~' : '.';
                const unsigned char c = (i % 3 == 0) ? e.color2 : e.color1;
                plotParticle(wx, wy, wz, g, c, true);
            }
            break;
        }
        case Emitter::Kind::Rain: {
            for (int i = 0; i < e.count; ++i) {
                const auto h = hash2D(i * 31 + static_cast<int>(ei) * 97, 733);
                const double ang = (h % 1000) * (kTwoPi / 1000.0);
                const double rad = e.radius * std::sqrt(((h >> 10) % 1000) / 1000.0);
                const double fall = std::fmod(tSafe * (0.48 + 0.08 * (i % 4)) * e.speed + ((h >> 20) % 1000) / 1000.0, 1.0);
                const double wx = e.x + rad * std::cos(ang) + fall * 0.35;
                const double wy = e.y + rad * std::sin(ang) + fall * 0.15;
                const double wz = e.zMax - fall * zSpan;
                const char g = (fall > 0.92) ? '.' : ((i % 3 == 0) ? '/' : '|');
                const unsigned char c = (i % 4 == 0) ? e.color2 : e.color1;
                plotParticle(wx, wy, wz, g, c, false);
            }
            break;
        }
        case Emitter::Kind::Spores: {
            for (int i = 0; i < e.count; ++i) {
                const auto h = hash2D(i * 19 + static_cast<int>(ei) * 53, 419);
                const double baseAng = (h % 1000) * (kTwoPi / 1000.0);
                const double baseRad = e.radius * std::sqrt(((h >> 10) % 1000) / 1000.0);
                const double phase = ((h >> 20) % 1000) * (kTwoPi / 1000.0);
                const double drift = tSafe * (0.35 + 0.06 * (i % 5)) * e.speed + phase;
                const double wx = e.x + baseRad * std::cos(baseAng + drift * 0.18) + 0.35 * std::sin(drift * 1.1);
                const double wy = e.y + baseRad * std::sin(baseAng + drift * 0.18) + 0.35 * std::cos(drift * 0.9);
                const double wz = e.zMin + zSpan * (0.5 + 0.45 * std::sin(drift * 0.75 + i));
                const double glow = std::sin(tSafe * 2.4 + phase);
                const char g = glow > 0.65 ? '*' : glow > 0.1 ? 'o' : glow > -0.4 ? '+' : '.';
                const unsigned char c = glow > 0.45 ? e.color2 : e.color1;
                plotParticle(wx, wy, wz, g, c, true);
            }
            break;
        }
        case Emitter::Kind::Geyser: {
            for (int i = 0; i < e.count; ++i) {
                const double rise = std::fmod(tSafe * (0.55 + 0.11 * (i % 5)) * e.speed + i * 0.381966, 1.0);
                const double ang = i * 2.399963 + tSafe * 2.2 + rise * 3.0;
                const double rad = e.radius * (0.12 + 0.88 * rise * rise);
                const double wx = e.x + rad * std::cos(ang);
                const double wy = e.y + rad * std::sin(ang);
                const double wz = e.zMin + std::sqrt(rise) * zSpan;
                const char g = rise < 0.28 ? '^' : rise < 0.58 ? '*' : rise < 0.82 ? '+' : '\'';
                const unsigned char c = rise < 0.4 ? e.color1 : e.color2;
                plotParticle(wx, wy, wz, g, c, true);
            }
            break;
        }
        case Emitter::Kind::Watchers: {
            // Spectral blinking eldritch eyes hovering in the mist!
            for (int i = 0; i < e.count; ++i) {
                const auto h = hash2D(i * 43 + static_cast<int>(ei) * 101, 613);
                const double ang = (h % 1000) * (kTwoPi / 1000.0) + tSafe * 0.04 * e.speed;
                const double rad = e.radius * (0.35 + 0.65 * (((h >> 10) % 1000) / 1000.0));
                const double wz = e.zMin + zSpan * (((h >> 20) % 1000) / 1000.0) + 0.18 * std::sin(tSafe * 1.1 + i);
                const double blink = std::sin(tSafe * (0.85 + 0.15 * (i % 4)) * e.speed + i * 1.7);
                if (blink < -0.25) continue; // blinked away into darkness
                const double wx = e.x + rad * std::cos(ang);
                const double wy = e.y + rad * std::sin(ang);
                const vec2 v(wx - posX, wy - posY);
                const double d = v * forward_;
                if (d < 0.2) continue;
                const double sx = screenWidth * 0.5 * (1.0 + (v * plane_) / (kPlaneWidth * kPlaneWidth * d));
                const double sy = horizon + (eye - wz) * screenHeight / d;
                const int cx = static_cast<int>(sx), cy = static_cast<int>(sy);
                if (cy < 0 || cy >= screenHeight) continue;
                const char pupil = blink > 0.55 ? 'O' : (blink > 0.1 ? 'o' : '-');
                const unsigned char pupilCol = blink > 0.55 ? e.color2 : e.color1;
                const float df = static_cast<float>(d);
                auto drawEyeCell = [&](int px, char g, unsigned char col) {
                    if (static_cast<unsigned>(px) >= static_cast<unsigned>(screenWidth)) return;
                    auto& depth = wallDepth[size_t(px) * screenHeight + cy];
                    if (df < depth) { depth = df; screen.drawCellUnchecked(px, cy, g, col); }
                };
                if (d < 16.0 && blink > 0.05) {
                    drawEyeCell(cx - 1, '<', e.color1);
                    drawEyeCell(cx, pupil, pupilCol);
                    drawEyeCell(cx + 1, '>', e.color1);
                } else {
                    drawEyeCell(cx, pupil, pupilCol);
                }
            }
            break;
        }
        case Emitter::Kind::Lightning: {
            // Periodic jagged vertical eldritch lightning strikes!
            const double period = std::max(1.2, 4.6 / e.speed);
            const double phase = std::fmod(tSafe + static_cast<double>(ei) * 1.37, period);
            if (phase > 0.34) break;
            const int strikeId = static_cast<int>((tSafe + static_cast<double>(ei) * 1.37) / period);
            const auto sh = hash2D(strikeId * 71 + static_cast<int>(ei) * 29, 887);
            const double ang = (sh % 1000) * (kTwoPi / 1000.0);
            const double rad = e.radius * (((sh >> 10) % 1000) / 1000.0);
            const double bx = e.x + rad * std::cos(ang);
            const double by = e.y + rad * std::sin(ang);
            const int segs = std::clamp<int>(e.count, 8, 64);
            const unsigned char boltCol = phase < 0.10 ? 15 : (phase < 0.22 ? e.color1 : e.color2);
            double jx = bx, jy = by;
            for (int s = 0; s < segs; ++s) {
                const double frac = static_cast<double>(s) / segs;
                const auto jh = hash2D(strikeId * 131 + s * 17, static_cast<int>(ei) * 41 + 9);
                const double stepX = (static_cast<int>(jh % 21) - 10) * 0.032;
                const double stepY = (static_cast<int>((jh >> 8) % 21) - 10) * 0.032;
                jx += stepX;
                jy += stepY;
                const double wz = e.zMax - frac * zSpan;
                const char g = (s % 5 == 0) ? '*' : ((jh & 1u) ? '/' : ((jh & 2u) ? '\\' : '|'));
                plotParticle(jx, jy, wz, g, boltCol, false);
            }
            break;
        }
        case Emitter::Kind::Halo: {
            // Rotating horizontal eldritch arcane sigil ring!
            for (int i = 0; i < e.count; ++i) {
                const double frac = static_cast<double>(i) / e.count;
                const double ang = frac * kTwoPi + tSafe * (0.75 * e.speed) * ((ei % 2 == 0) ? 1.0 : -1.0);
                const double pulseR = e.radius * (1.0 + 0.06 * std::sin(tSafe * 2.2 + frac * kTwoPi * 3.0));
                const double wx = e.x + pulseR * std::cos(ang);
                const double wy = e.y + pulseR * std::sin(ang);
                const double wz = e.zMin + zSpan * (0.5 + 0.45 * std::sin(frac * kTwoPi * 2.0 + tSafe * 1.4));
                static constexpr char kSigils[] = {'*', '+', 'X', 'o', ':', '~'};
                const char g = kSigils[i % 6];
                const unsigned char c = (i % 2 == 0) ? e.color1 : e.color2;
                plotParticle(wx, wy, wz, g, c, true);
            }
            break;
        }
        }
    }
    if (!other || !validPose(*other)) return;
    const vec2 offset(other->x-posX,other->y-posY);
    const double depth=offset*forward_;
    if(depth<=0.05) return;
    const double h=screenHeight/depth, w=screenWidth*0.25/(kPlaneWidth*depth);
    const double left=screenWidth*0.5*(1+(offset*plane_)/(kPlaneWidth*kPlaneWidth*depth))-w*0.5;
    const double top=horizon+(eye-other->feet-1)*h;
    const double relative=other->yaw-std::atan2(dirY,dirX);
    const char facing=std::abs(std::sin(relative))>0.7?(std::sin(relative)>0?'<':'>'):(std::cos(relative)>0?'O':'@');
    for(int x=edge(left,screenWidth);x<edge(left+w,screenWidth);++x)
        for(int y=edge(top,screenHeight);y<edge(top+h,screenHeight);++y){
            if(depth>=wallDepth[size_t(x)*screenHeight+y]) continue;
            const char c=playerGlyph((x+0.5-left)/w,(y+0.5-top)/h,facing);
            if(c!=' ') screen.drawCell(x,y,c,13);
        }
}
