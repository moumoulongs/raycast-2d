// Run from the repository root. Hashing and scene setup are outside the timed section.
#include "camera.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
    using Clock = std::chrono::steady_clock;
    for (const char* file : {"assets/maps/foundry.map", "assets/maps/skyway.map"}) {
        WorldMap map(file, "assets/textures/industrial.atlas");
        Camera camera(map.spawnPosition(), map.spawnDirection());
        Screen screen(160, 60);
        constexpr int frames = 120;
        std::array<double, 7> times{};
        std::uint64_t reference = 0;
        for (int round = -1; round < int(times.size()); ++round) {
            camera = Camera(map.spawnPosition(), map.spawnDirection());
            double elapsed = 0;
            std::uint64_t hash = 14695981039346656037ULL;
            for (int i = 0; i < frames; ++i) {
                camera.turn(0.024);
                camera.look(0, i % 40 < 20 ? 2 : -2);
                screen.clear();
                auto start = Clock::now();
                camera.render(map, screen);
                elapsed += std::chrono::duration<double, std::micro>(Clock::now() - start).count();
                for (int y = 1; y <= screen.height(); ++y) {
                    for (int x = 1; x <= screen.width(); ++x) {
                        const auto cell = screen.framedCell(x, y);
                        hash = (hash ^ static_cast<unsigned char>(cell.glyph)) * 1099511628211ULL;
                        hash = (hash ^ cell.color) * 1099511628211ULL;
                    }
                }
            }
            if (round < 0) reference = hash;
            else {
                if (reference != hash) return 1;
                times[round] = elapsed / frames;
            }
        }
        std::sort(times.begin(), times.end());
        std::cout << file << " median_us/frame=" << times[times.size()/2]
                  << " glyph_color_fnv64=" << std::hex << reference << std::dec << '\n';
    }
}
