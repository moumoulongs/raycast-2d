#ifdef _WIN32

#include "platform.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <conio.h>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

namespace {

class WindowsTerminalPlatform final : public Platform {
public:
    WindowsTerminalPlatform() {
        configureTerminal();
    }

    ~WindowsTerminalPlatform() override {
        std::cout << "\x1b[?25h" << std::flush;
        if (fontChanged_) SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &originalFont_);
    }

    int width() const override {
        return width_;
    }

    int height() const override {
        return height_;
    }

    InputState pollInput() override {
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        DWORD pending = 0;
        bool quit = false;
        bool pressed[4]{};
        if (!GetNumberOfConsoleInputEvents(input, &pending)) return {0, 0, true};
        while (pending > 0) {
            INPUT_RECORD events[32];
            DWORD count = 0;
            if (!ReadConsoleInputW(input, events, std::min<DWORD>(32, pending), &count) || !count)
                return {0, 0, true};
            pending -= count;
            for (DWORD i = 0; i < count; ++i) {
                const auto& event = events[i];
                if (event.EventType == FOCUS_EVENT && !event.Event.FocusEvent.bSetFocus) {
                    std::fill_n(held_, 4, false);
                    std::fill_n(pressed, 4, false);
                }
                if (event.EventType != KEY_EVENT) continue;
                const auto& key = event.Event.KeyEvent;
                int slot;
                switch (key.wVirtualKeyCode) {
                case 'W': slot = 0; break;
                case 'S': slot = 1; break;
                case 'A': slot = 2; break;
                case 'D': slot = 3; break;
                case 'Q': case VK_ESCAPE: quit |= key.bKeyDown != 0; continue;
                default: continue;
                }
                pressed[slot] |= key.bKeyDown && !held_[slot];
                held_[slot] = key.bKeyDown != 0;
            }
        }
        auto active = [&](int i) { return held_[i] || pressed[i]; };
        return {active(0) - active(1), active(2) - active(3), quit};
    }

    void present(const Screen& screen) override {
        const std::string frame = screen.toDisplayString();
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(output, &info)) {
            std::cout << "\x1b[H" << frame << std::flush;
            return;
        }

        // A stream write to the bottom-right cell can wrap and scroll the frame.
        // Write visible cells directly, relative to the current viewport.
        const int stride = screen.width() + 3; // two borders and a newline
        const int columns = std::min(stride - 1, info.srWindow.Right - info.srWindow.Left + 1);
        const int rows = std::min(screen.height() + 2, info.srWindow.Bottom - info.srWindow.Top + 1);
        cells_.resize(static_cast<std::size_t>(columns) * rows);
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < columns; ++x) {
                auto& cell = cells_[y * columns + x];
                cell.Char.UnicodeChar = frame[y * stride + x];
                cell.Attributes = info.wAttributes;
            }
        }
        SMALL_RECT area = info.srWindow;
        area.Right = static_cast<SHORT>(area.Left + columns - 1);
        area.Bottom = static_cast<SHORT>(area.Top + rows - 1);
        WriteConsoleOutputW(output, cells_.data(), {static_cast<SHORT>(columns), static_cast<SHORT>(rows)},
                            {0, 0}, &area);
    }

private:
    CONSOLE_FONT_INFOEX originalFont_{sizeof(CONSOLE_FONT_INFOEX)};
    bool fontChanged_ = false;
    bool held_[4]{};
    void configureTerminal() {
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetCurrentConsoleFontEx(output, FALSE, &originalFont_) && originalFont_.dwFontSize.Y > 10) {
            auto smaller = originalFont_;
            smaller.dwFontSize.Y = static_cast<SHORT>(std::max(10, originalFont_.dwFontSize.Y * 4 / 5));
            smaller.dwFontSize.X = 0; // Let the host choose the matching glyph width.
            fontChanged_ = SetCurrentConsoleFontEx(output, FALSE, &smaller) != FALSE;
        }
        DWORD outputMode = 0;
        if (GetConsoleMode(output, &outputMode)) {
            outputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(output, outputMode);
        }

        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (GetConsoleScreenBufferInfo(output, &info)) {
            const int columns = info.srWindow.Right - info.srWindow.Left + 1;
            const int rows = info.srWindow.Bottom - info.srWindow.Top + 1;
            width_ = std::max(20, columns - 2);
            height_ = std::max(10, rows - 2);
        }

        std::cout << "\x1b[2J\x1b[H\x1b[?25l" << std::flush;
    }

    std::vector<CHAR_INFO> cells_;
    int width_ = 118;
    int height_ = 38;
};

} // namespace

std::unique_ptr<Platform> createPlatform() {
    return std::make_unique<WindowsTerminalPlatform>();
}

#endif
