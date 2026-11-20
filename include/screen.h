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

    // Crisp UTF-8 text overlay rendered after ASCII blit (enables sharp bilingual ZH/EN UI
    // without enlarging Screen::Cell or touching 3D raycasting hot paths).
    struct TextOverlay {
        int cellX = 0;
        int cellY = 0;
        int cellW = 0;
        int cellH = 0; // 0 = single line; > 0 = multi-line wrapped box height in cells
        std::string utf8;
        unsigned char color = 15;
        bool centered = false;
        bool large = false;
    };

    void clear();
    void resize(int width, int height);
    void addOverlay(int cellX, int cellY, int cellW, std::string utf8,
                    unsigned char color, bool centered = false, bool large = false, int cellH = 0) {
        overlays_.push_back({cellX, cellY, cellW, cellH, std::move(utf8), color, centered, large});
    }
    const std::vector<TextOverlay>& overlays() const { return overlays_; }
    // Colors 0..7 follow ANSI order (black through white); 8..15 are bright.
    void drawCell(int x, int y, char glyph, unsigned char color) {
        if (static_cast<unsigned>(x) >= static_cast<unsigned>(width_) ||
            static_cast<unsigned>(y) >= static_cast<unsigned>(height_)) {
            return;
        }
        canvas_[static_cast<std::size_t>(y) * stride_ + x] = {glyph, color < DefaultColor ? color : DefaultColor};
    }
    void drawCellUnchecked(int x, int y, char glyph, unsigned char color) {
        canvas_[static_cast<std::size_t>(y) * stride_ + x] = {glyph, color < DefaultColor ? color : DefaultColor};
    }
    Cell framedCell(int x, int y) const; // includes the one-cell border
    const Cell* data() const { return canvas_.data(); }
    int stride() const { return stride_; } // 32-cell (64-byte) aligned row stride
    std::string toAnsiString() const;

    int width() const { return width_; }
    int height() const { return height_; }
    std::string toDisplayString() const;

private:
    std::string formatFrame(bool ansi) const;

    int width_;
    int height_;
    int stride_;
    std::vector<Cell> canvas_;
    std::vector<TextOverlay> overlays_;
};
