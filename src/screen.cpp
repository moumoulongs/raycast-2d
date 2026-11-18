#include "screen.h"

#include <algorithm>
#include <cstdlib>

Screen::Screen(int width, int height)
    : width_(std::max(1, width)),
      height_(std::max(1, height)),
      canvas_(static_cast<std::size_t>(width_) * height_) {
    clear();
}

void Screen::clear() {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            drawCell(x, y, ' ', DefaultColor);
        }
    }
}

void Screen::drawPoint(int x, int y, std::uint8_t luminance) {
    drawCell(x, y, brightness(luminance), DefaultColor);
}

void Screen::drawCell(int x, int y, char glyph, unsigned char color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }

    canvas_[static_cast<std::size_t>(y) * width_ + x] = {glyph, std::min(color, DefaultColor)};
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

Screen::Cell Screen::framedCell(int x, int y) const {
    if (x < 0 || y < 0 || x > width_ + 1 || y > height_ + 1) return {' ', DefaultColor};
    const bool side = x == 0 || x == width_ + 1;
    const bool end = y == 0 || y == height_ + 1;
    if (side || end) return {side && end ? '+' : '@', DefaultColor};
    return canvas_[static_cast<std::size_t>(y - 1) * width_ + x - 1];
}

std::string Screen::toDisplayString() const {
    return formatFrame(false);
}

std::string Screen::toAnsiString() const {
    return formatFrame(true);
}

std::string Screen::formatFrame(bool ansi) const {
    std::string frame;
    frame.reserve((static_cast<std::size_t>(width_) + 3) * (height_ + 2));
    unsigned char previous = DefaultColor;
    for (int y = 0; y < height_ + 2; ++y) {
        if (y) frame.push_back('\n');
        for (int x = 0; x < width_ + 2; ++x) {
            const auto cell = framedCell(x, y);
            if (ansi && previous != cell.color) {
                const int code = cell.color == DefaultColor ? 39 :
                                 (cell.color < 8 ? 30 : 90) + cell.color % 8;
                frame += "\x1b[" + std::to_string(code) + 'm';
                previous = cell.color;
            }
            frame.push_back(cell.glyph);
        }
    }
    return frame;
}
