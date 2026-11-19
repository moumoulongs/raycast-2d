#ifndef _WIN32

#include "platform.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <string>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace {

class PosixTerminalPlatform final : public Platform {
public:
    PosixTerminalPlatform() {
        configureTerminal();
    }

    ~PosixTerminalPlatform() override {
        tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios_);
        std::cout << "\x1b[?25h\x1b[0m" << std::flush;
    }

    int width() const override {
        return width_;
    }

    int height() const override {
        return height_;
    }

    InputState pollInput() override {
        InputState state;
        char keys[64];
        const ssize_t n = ::read(STDIN_FILENO, keys, sizeof(keys));
        if (n > 0) pending_.append(keys, static_cast<std::size_t>(n));
        state.consoleConsumed = console_;
        std::size_t i = 0;
        while (i < pending_.size()) {
            const char key = pending_[i];
            if (key == 27) {
                const auto left = pending_.size() - i;
                if (left == 1 && n > 0) break; // Allow the rest of a split arrow sequence to arrive.
                if (left > 1 && (pending_[i+1] == '[' || pending_[i+1] == 'O')) {
                    auto end = i + 2;
                    while (end < pending_.size() && (pending_[end] < '@' || pending_[end] > '~')) ++end;
                    if (end == pending_.size() && left < 64 && n > 0) break;
                    if (end < pending_.size() && console_) {
                        if (pending_[end] == 'A') state.consoleKeys += ConsoleKey::Previous;
                        if (pending_[end] == 'B') state.consoleKeys += ConsoleKey::Next;
                    }
                    i = std::min(end + 1, pending_.size());
                    continue;
                }
            }
            ++i;
            if (key == '`' || key == '~' || (console_ && key == 27)) {
                console_ = !console_;
                state.consoleConsumed = true;
                state.move = state.strafe = 0;
                state.jump = state.exit = false;
                state.consoleKeys += ConsoleKey::Reset;
                continue;
            }
            if (console_) {
                if (key == '\r' || key == '\n') state.consoleKeys += '\n';
                else if (key == 127 || key == '\b') state.consoleKeys += '\b';
                else if (key >= ' ' && key <= '~') state.consoleKeys += key;
                continue;
            }
            if (state.consoleConsumed) continue;
            switch (key) {
            case ' ': state.jump = true; break;
            case 'w': case 'W': state.move = 1; break;
            case 's': case 'S': state.move = -1; break;
            case 'a': case 'A': state.strafe = 1; break;
            case 'd': case 'D': state.strafe = -1; break;
            case 'q': case 'Q': case 27: state.exit = true; break;
            }
        }
        pending_.erase(0, i);
        state.consoleActive = console_;
        return state;
    }

    void present(const Screen& screen) override {
        std::cout << "\x1b[H" << screen.toAnsiString() << std::flush;
    }

private:
    void configureTerminal() {
        tcgetattr(STDIN_FILENO, &originalTermios_);
        termios raw = originalTermios_;
        raw.c_lflag &= static_cast<unsigned long>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);

        const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        }

        winsize size{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) {
            width_ = std::max(20, static_cast<int>(size.ws_col) - 2);
            height_ = std::max(10, static_cast<int>(size.ws_row) - 2);
        }

        std::cout << "\x1b[2J\x1b[H\x1b[?25l" << std::flush;
    }

    bool console_ = false;
    std::string pending_;
    termios originalTermios_{};
    int width_ = 118;
    int height_ = 38;
};

} // namespace

std::unique_ptr<Platform> createPlatform(int /*fontHeight*/) {
    return std::make_unique<PosixTerminalPlatform>();
}

#endif
