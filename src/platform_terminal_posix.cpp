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
        for (ssize_t i = 0; i < n; ++i) {
            switch (keys[i]) {
            case ' ': state.jump = true; break;
            case 'w': case 'W': state.move = 1; break;
            case 's': case 'S': state.move = -1; break;
            case 'a': case 'A': state.strafe = 1; break;
            case 'd': case 'D': state.strafe = -1; break;
            case 'q': case 'Q': case 27: state.exit = true; break;
            }
        }
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

    termios originalTermios_{};
    int width_ = 118;
    int height_ = 38;
};

} // namespace

std::unique_ptr<Platform> createPlatform(int /*fontHeight*/) {
    return std::make_unique<PosixTerminalPlatform>();
}

#endif
