#pragma once

struct InputState {
    int move = 0; // forward minus backward
    int strafe = 0; // left minus right
    bool exit = false;
    bool jump = false;
    int mouseX = 0, mouseY = 0; // drag delta in terminal cells
};
