#pragma once

#include <string>
#include <vector>

class Screen {
public:
    Screen(int width, int height);

    struct Cell { char glyph; unsigned char color; };
    static constexpr unsigned char DefaultColor = 16; // terminal's normal foreground

    void clear();
    // Colors 0..7 follow ANSI order (black through white); 8..15 are bright.
    void drawCell(int x, int y, char glyph, unsigned char color);
    Cell framedCell(int x, int y) const; // includes the one-cell border
    std::string toAnsiString() const;

    int width() const;
    int height() const;
    std::string toDisplayString() const;

private:
    std::string formatFrame(bool ansi) const;

    int width_;
    int height_;
    std::vector<Cell> canvas_;
};
