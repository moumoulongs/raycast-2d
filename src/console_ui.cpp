#include "console_ui.h"
#include <algorithm>

void ConsoleUi::log(const std::string& message) {
    if (message.empty()) return;
    std::string text;
    auto append = [&] {
        if (lines_.size() == 128) lines_.pop_front();
        lines_.push_back(text);
        text.clear();
    };
    for (unsigned char c : message) {
        if (c == '\n') append();
        else if (text.size() < 1024) text += c >= 32 && c < 127 ? static_cast<char>(c) : '?';
    }
    if (!text.empty()) append();
}

void ConsoleUi::update(const InputState& input, const Handler& execute) {
    for (char key : input.consoleKeys) {
        if (key == ConsoleKey::Reset || key == '\n') {
            if (key == '\n' && line_.find_first_not_of(' ') != std::string::npos) {
                if (history_.empty() || history_.back() != line_) {
                    if (history_.size() == 64) history_.pop_front();
                    history_.push_back(line_);
                }
                log("> " + line_);
                if (line_ == "clear") lines_.clear();
                else if (line_ == "history") for (const auto& item : history_) log(item);
                else log(execute(line_));
            }
            line_.clear(); draft_.clear(); back_ = 0;
        } else if (key == ConsoleKey::Previous || key == ConsoleKey::Next) {
            if (!back_) draft_ = line_;
            if (key == ConsoleKey::Previous && back_ < history_.size()) ++back_;
            if (key == ConsoleKey::Next && back_) --back_;
            line_ = back_ ? history_[history_.size() - back_] : draft_;
        } else if (key == '\b') {
            if (!line_.empty()) line_.pop_back();
        } else if (key >= ' ' && key <= '~' && line_.size() < 512) line_ += key;
    }
}

void ConsoleUi::draw(Screen& screen) const {
    const int width = screen.width(), rows = std::min(screen.height(), std::max(5, screen.height()/2));
    auto row = [&](int y, const std::string& text, unsigned char color) {
        for (int x = 0; x < width; ++x)
            screen.drawCell(x, y, x < static_cast<int>(text.size()) ? text[x] : ' ', color);
    };
    row(0, "CONSOLE | help: commands | Up/Down: history | Esc: close", 11);
    int y = rows - 2;
    for (auto i = lines_.rbegin(); i != lines_.rend() && y > 0; ++i) {
        int start = static_cast<int>(i->empty() ? 0 : (i->size()-1) / width * width);
        for (; start >= 0 && y > 0; start -= width) row(y--, i->substr(start, width), 7);
    }
    while (y > 0) row(y--, "", 7);
    auto tail = static_cast<std::size_t>(std::max(0, width - 3));
    row(rows - 1, "> " + line_.substr(line_.size() > tail ? line_.size()-tail : 0) + "_", 14);
}
