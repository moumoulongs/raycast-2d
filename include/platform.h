#pragma once

#include <memory>
#include <optional>

#include "input.h"
#include "screen.h"

class Platform {
public:
    virtual ~Platform() = default;

    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual InputState pollInput() = 0;
    virtual void present(const Screen& screen) = 0;
};

std::unique_ptr<Platform> createPlatform();
