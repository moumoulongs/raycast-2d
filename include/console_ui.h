#pragma once
#include "input.h"
#include "screen.h"
#include <deque>
#include <functional>

class ConsoleUi {
public:
    using Handler = std::function<std::string(const std::string&)>;
    void update(const InputState& input, const Handler& execute);
    void log(const std::string& message);
    void draw(Screen& screen) const;
private:
    std::deque<std::string> lines_, history_;
    std::string line_, draft_;
    std::size_t back_ = 0;
};
