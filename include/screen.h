#pragma once

#include <algorithm>
#include <string>
#include <vector>

class Screen {
public:
    Screen(int width, int height);

    struct Cell { char glyph; unsigned char color; };
    static_assert(sizeof(Cell) == 2, "Screen::Cell must remain 2 bytes");
    static constexpr unsigned char DefaultColor = 16; // terminal's normal foreground

    void clear();
    void resize(int width, int height);
    // Colors 0..7 follow ANSI order (black through white); 8..15 are bright.
    void drawCell(int x, int y, char glyph, unsigned char color) {
        if (static_cast<unsigned>(x) >= static_cast<unsigned>(width_) ||
            static_cast<unsigned>(y) >= static_cast<unsigned>(height_)) {
            return;
        }
        canvas_[static_cast<std::size_t>(y) * width_ + x] = {glyph, color < DefaultColor ? color : DefaultColor};
    }
    void drawCellUnchecked(int x, int y, char glyph, unsigned char color) {
        canvas_[static_cast<std::size_t>(y) * width_ + x] = {glyph, color < DefaultColor ? color : DefaultColor};
    }
    Cell framedCell(int x, int y) const; // includes the one-cell border
    const Cell* data() const { return canvas_.data(); }
    std::string toAnsiString() const;

    int width() const { return width_; }
    int height() const { return height_; }
    std::string toDisplayString() const;

private:
    std::string formatFrame(bool ansi) const;

    int width_;
    int height_;
    std::vector<Cell> canvas_;
};
