#pragma once

struct InputState {
    int move = 0; // forward minus backward
    int turn = 0; // left minus right
    bool exit = false;
};
