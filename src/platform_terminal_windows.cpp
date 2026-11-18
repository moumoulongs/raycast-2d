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
#include "mouse_capture.h"

namespace {

class WindowsTerminalPlatform final : public Platform {
public:
    explicit WindowsTerminalPlatform(int fontHeight) {
        configureTerminal(fontHeight);
        HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
        if (GetConsoleMode(in,&savedInputMode_))
            mouseEnabled_ = SetConsoleMode(in,(savedInputMode_ | ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT) & ~ENABLE_QUICK_EDIT_MODE) != FALSE;
    }

    ~WindowsTerminalPlatform() override {
        mouse_.release();
        std::cout << "\x1b[?25h" << std::flush;
        if (mouseEnabled_) SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),savedInputMode_);
        if (fontChanged_) {
            const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleWindowInfo(out, TRUE, &originalView_);
            SetCurrentConsoleFontEx(out, FALSE, &originalFont_);
            SetConsoleWindowInfo(out, TRUE, &originalView_);
        }
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
        int dx = 0, dy = 0;
        bool pressed[5]{};
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
                    mouse_.release();
                    dx = dy = 0;
                    std::fill_n(held_, 5, false);
                    std::fill_n(pressed, 5, false);
                }
                if (mouseEnabled_ && event.EventType == MOUSE_EVENT) {
                    const auto& m = event.Event.MouseEvent;
                    const bool down = (m.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0;
                    if (down && m.dwEventFlags == 0) mouse_.acquire();
                    continue;
                }
                if (event.EventType != KEY_EVENT) continue;
                const auto& key = event.Event.KeyEvent;
                int slot;
                switch (key.wVirtualKeyCode) {
                case VK_SPACE: slot = 4; break;
                case 'W': slot = 0; break;
                case 'S': slot = 1; break;
                case 'A': slot = 2; break;
                case 'D': slot = 3; break;
                case VK_ESCAPE: if(key.bKeyDown) mouse_.release(); continue;
                case 'Q': quit |= key.bKeyDown != 0; continue;
                default: continue;
                }
                pressed[slot] |= key.bKeyDown && !held_[slot];
                held_[slot] = key.bKeyDown != 0;
            }
        }
        mouse_.poll(dx,dy);
        auto active = [&](int i) { return held_[i] || pressed[i]; };
        return {active(0) - active(1), active(2) - active(3), quit, pressed[4], dx, dy};
    }

    void present(const Screen& screen) override {
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(output, &info)) {
            std::cout << "\x1b[H" << screen.toAnsiString() << std::flush;
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
                const auto pixel = screen.framedCell(x, y);
                cell.Char.UnicodeChar = static_cast<unsigned char>(pixel.glyph);
                cell.Attributes = info.wAttributes;
                if (pixel.color != Screen::DefaultColor) {
                    const unsigned c = pixel.color;
                    cell.Attributes = static_cast<WORD>((info.wAttributes & ~0x0f) |
                        ((c & 1) ? FOREGROUND_RED : 0) | ((c & 2) ? FOREGROUND_GREEN : 0) |
                        ((c & 4) ? FOREGROUND_BLUE : 0) | ((c & 8) ? FOREGROUND_INTENSITY : 0));
                }
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
    DWORD savedInputMode_{};
    MouseCapture mouse_;
    bool mouseEnabled_ = false;
    SMALL_RECT originalView_{};
    bool fontChanged_ = false;
    bool held_[5]{};
    void configureTerminal(int fontHeight) {
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO before{};
        if (GetConsoleScreenBufferInfo(output, &before) && GetCurrentConsoleFontEx(output, FALSE, &originalFont_) && fontHeight > 0 && originalFont_.dwFontSize.Y > fontHeight) {
            auto smaller = originalFont_;
            smaller.dwFontSize.Y = static_cast<SHORT>(fontHeight);
            smaller.dwFontSize.X = 0; // Let the host choose the matching glyph width.
            originalView_ = before.srWindow;
            fontChanged_ = SetCurrentConsoleFontEx(output, FALSE, &smaller) != FALSE;
            CONSOLE_FONT_INFOEX actual{sizeof(CONSOLE_FONT_INFOEX)};
            CONSOLE_SCREEN_BUFFER_INFO after{};
            if (fontChanged_ && GetCurrentConsoleFontEx(output,FALSE,&actual) && GetConsoleScreenBufferInfo(output,&after)) {
                const COORD limit = GetLargestConsoleWindowSize(output);
                auto extent = [](int cells,int oldPixel,int newPixel,int maximum) {
                    return static_cast<SHORT>(std::clamp(cells * oldPixel / std::max(1,newPixel),1,std::max(1,maximum)));
                };
                SHORT w = extent(originalView_.Right-originalView_.Left+1,originalFont_.dwFontSize.X,actual.dwFontSize.X,limit.X);
                SHORT h = extent(originalView_.Bottom-originalView_.Top+1,originalFont_.dwFontSize.Y,actual.dwFontSize.Y,limit.Y);
                COORD buffer{std::max(w,after.dwSize.X),std::max(h,after.dwSize.Y)};
                SMALL_RECT view{0,0,static_cast<SHORT>(w-1),static_cast<SHORT>(h-1)};
                if (SetConsoleScreenBufferSize(output,buffer)) SetConsoleWindowInfo(output,TRUE,&view);
            }
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

std::unique_ptr<Platform> createPlatform(int fontHeight) {
    return std::make_unique<WindowsTerminalPlatform>(fontHeight);
}

#endif
