#include "screen.h"

#include <algorithm>
#include <cstdlib>

Screen::Screen(int width, int height)
    : width_(std::max(1, width)),
      height_(std::max(1, height)),
      canvas_(static_cast<std::size_t>(width_ * height_), 0) {
    clear();
}

void Screen::clear() {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            canvas_[static_cast<std::size_t>(y * width_ + x)] = (y >= height_ / 2) ? 15 : 0;
        }
    }
}

void Screen::drawPoint(int x, int y, std::uint8_t luminance) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }

    canvas_[static_cast<std::size_t>(y * width_ + x)] = luminance;
}

void Screen::drawLine(int x0, int y0, int x1, int y1, std::uint8_t luminance) {
    if (x0 < 0 || y0 < 0 || x0 >= width_ || y0 >= height_) {
        return;
    }
    if (x1 < 0 || y1 < 0 || x1 >= width_ || y1 >= height_) {
        return;
    }

    const int dx = std::abs(x1 - x0);
    const int sx = (x0 < x1) ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = (y0 < y1) ? 1 : -1;
    int error = dx + dy;

    while (true) {
        drawPoint(x0, y0, luminance);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        const int doubledError = 2 * error;
        if (doubledError >= dy) {
            error += dy;
            x0 += sx;
        }
        if (doubledError <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

int Screen::width() const {
    return width_;
}

int Screen::height() const {
    return height_;
}

char Screen::brightness(std::uint8_t value) {
    static constexpr char kRamp[] = " .,^:-+abcdwf$&%#@";
    return kRamp[value * 18 / 256];
}

std::string Screen::toDisplayString() const {
    std::string frame;
    frame.reserve(static_cast<std::size_t>((width_ + 3) * (height_ + 2)));

    frame.push_back('+');
    frame.append(static_cast<std::size_t>(width_), '@');
    frame.push_back('+');
    frame.push_back('\n');

    for (int y = 0; y < height_; ++y) {
        frame.push_back('@');
        for (int x = 0; x < width_; ++x) {
            frame.push_back(brightness(canvas_[static_cast<std::size_t>(y * width_ + x)]));
        }
        frame.push_back('@');
        frame.push_back('\n');
    }

    frame.push_back('+');
    frame.append(static_cast<std::size_t>(width_), '@');
    frame.push_back('+');

    return frame;
}
