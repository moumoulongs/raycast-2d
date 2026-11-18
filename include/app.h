#pragma once

#include <memory>

#include "camera.h"
#include "platform.h"
#include "screen.h"
#include "world_map.h"

class App {
public:
    explicit App(std::unique_ptr<Platform> platform);

    int run();

private:
    std::unique_ptr<Platform> platform_;
    WorldMap worldMap_;
    Camera camera_;
    Screen screen_;
};
