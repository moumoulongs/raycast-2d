#include "app.h"

#include <algorithm>
#include <chrono>
#include <thread>

App::App(std::unique_ptr<Platform> platform, WorldMap map)
    : platform_(std::move(platform)),
      worldMap_(std::move(map)),
      camera_(worldMap_.spawnPosition(), worldMap_.spawnDirection()),
      screen_(platform_->width(), platform_->height()) {}

int App::run() {
    using Clock = std::chrono::steady_clock;
    constexpr auto period = std::chrono::microseconds(16667);
    auto previous = Clock::now() - period;
    for (;;) {
        const auto start = Clock::now();
        const double dt = std::min(0.05, std::chrono::duration<double>(start - previous).count());
        previous = start;
        const auto input = platform_->pollInput();
        if (input.exit) break;
        camera_.look(input.mouseX,input.mouseY);

        camera_.advance(worldMap_, dt, input.move, input.jump, input.strafe);

        screen_.clear();
        camera_.render(worldMap_, screen_);
        platform_->present(screen_);
        std::this_thread::sleep_until(start + period);
    }

    return 0;
}
