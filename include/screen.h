#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Screen {
public:
    Screen(int width, int height);

    void clear();
    void drawPoint(int x, int y, std::uint8_t luminance);
    void drawLine(int x0, int y0, int x1, int y1, std::uint8_t luminance);

    int width() const;
    int height() const;
    std::string toDisplayString() const;

private:
    static char brightness(std::uint8_t value);

    int width_;
    int height_;
    std::vector<std::uint8_t> canvas_;
};
