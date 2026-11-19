#include "screen.h"

#include <algorithm>

Screen::Screen(int width, int height)
    : width_(std::max(1, width)),
      height_(std::max(1, height)),
      canvas_(static_cast<std::size_t>(width_) * height_) {
    clear();
}

void Screen::clear() {
    std::fill(canvas_.begin(), canvas_.end(), Cell{' ', DefaultColor});
}

void Screen::drawCell(int x, int y, char glyph, unsigned char color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }

    canvas_[static_cast<std::size_t>(y) * width_ + x] = {glyph, std::min(color, DefaultColor)};
}

int Screen::width() const {
    return width_;
}

int Screen::height() const {
    return height_;
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
