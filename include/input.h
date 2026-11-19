#pragma once

#include <string>

namespace ConsoleKey {
constexpr char Reset = 1, Previous = 2, Next = 3;
}

struct InputState {
    int move = 0; // forward minus backward
    int strafe = 0; // left minus right
    bool exit = false;
    bool jump = false;
    int mouseX = 0, mouseY = 0; // look delta
    bool consoleActive = false, consoleConsumed = false;
    std::string consoleKeys;
};
