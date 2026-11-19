#ifdef _WIN32

#include "platform.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <windows.h>

namespace {

// Tuned 16-color BGRX palette (0x00RRGGBB in little-endian DIBSection).
constexpr std::array<std::uint32_t, 17> kPalette = {
    0x0005070Au, // 0: Abyssal Black
    0x00961E2Au, // 1: Dark Blood Red
    0x00187046u, // 2: Deep Sea Green
    0x00A68232u, // 3: Tarnished Bronze
    0x00164078u, // 4: Trench Blue
    0x00722E7Au, // 5: Occult Purple
    0x001E8484u, // 6: R'lyeh Teal
    0x00A4AEB8u, // 7: Basalt Silver
    0x00485460u, // 8: Obsidian Gray
    0x00FF3B4Eu, // 9: Eldritch Crimson
    0x0038FF88u, // 10: Bioluminescent Emerald
    0x00FFE26Au, // 11: Ritual Gold
    0x004C94FFu, // 12: Electric Deep Blue
    0x00E258FFu, // 13: Astral Violet
    0x0042F4FFu, // 14: Luminous Cyan
    0x00F4F8FFu, // 15: Starlight White
    0x00C8D6E0u, // 16: Default
};

constexpr std::uint32_t kBgColor = 0x0005070Au;

inline std::uint16_t packCell(Screen::Cell c) {
    return static_cast<std::uint16_t>(static_cast<unsigned char>(c.glyph) | (static_cast<unsigned>(c.color) << 8));
}

class Win32WindowPlatform final : public Platform {
public:
    explicit Win32WindowPlatform(int fontHeight) {
        bakeGlyphAtlas(fontHeight > 0 ? fontHeight : 6);
        createAppWindow();
    }

    ~Win32WindowPlatform() override {
        releaseMouse();
        destroyDib();
        if (hwnd_) DestroyWindow(hwnd_);
    }

    int width() const override { return width_; }
    int height() const override { return height_; }

    InputState pollInput() override {
        resetFrameEvents();
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                quit_ = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (mouseLocked_ && GetForegroundWindow() != hwnd_) releaseMouse();

        InputState s{};
        s.viewportWidth = width_;
        s.viewportHeight = height_;
        s.consoleActive = console_;
        s.consoleConsumed = console_ || consoleToggled_;
        s.consoleKeys = std::move(consoleKeys_);
        s.mouseLocked = mouseLocked_;
        s.exit = quit_;
        if (s.consoleConsumed) return s;

        auto active = [&](int i) { return held_[i] || pressed_[i]; };
        s.move = active(0) - active(1);
        s.strafe = active(2) - active(3);
        s.jump = pressed_[4];
        s.mouseX = mouseDx_;
        s.mouseY = mouseDy_;
        s.toggleHud = toggleHud_;
        s.toggleMinimap = toggleMinimap_;
        s.toggleMenu = toggleMenu_;
        s.menuUp = menuUp_;
        s.menuDown = menuDown_;
        s.menuConfirm = menuConfirm_;
        return s;
    }

    void present(const Screen& screen) override {
        if (!hwnd_ || !dibPixels_ || !memDc_) return;
        const int sw = screen.width();
        const int sh = screen.height();
        const int cols = std::min(sw, width_);
        const int rows = std::min(sh, height_);
        if (cols <= 0 || rows <= 0) return;

        const std::size_t total = static_cast<std::size_t>(cols) * rows;
        const bool full = (prevCols_ != cols || prevRows_ != rows || prevCells_.size() != total);
        if (full) {
            prevCells_.assign(total, 0xFFFFu);
            prevCols_ = cols;
            prevRows_ = rows;
            std::fill_n(dibPixels_, static_cast<std::size_t>(pixelW_) * pixelH_, kBgColor);
        }

        const Screen::Cell* const src = screen.data();
        const int gw = glyphW_;
        const int gh = glyphH_;
        const int stride = pixelW_;
        int dirtyMinX = cols, dirtyMaxX = -1;
        int dirtyMinY = rows, dirtyMaxY = -1;

        auto blitCell = [&](std::uint32_t* dst, unsigned char ch, unsigned char color) {
            if (ch <= 32 || ch > 126) {
                for (int gy = 0; gy < gh; ++gy) {
                    std::uint32_t* p = dst + static_cast<std::size_t>(gy) * stride;
                    for (int gx = 0; gx < gw; ++gx) p[gx] = kBgColor;
                }
                return;
            }
            const std::uint32_t fg = kPalette[color < 17 ? color : 16];
            const std::uint8_t* const mask = glyphMasks_.data() + static_cast<std::size_t>(ch - 32) * gh;
            if (gw == 4) {
                // Exact 16-byte (128-bit) unrolled branchless fast path for 4px-wide glyphs!
                for (int gy = 0; gy < gh; ++gy) {
                    std::uint32_t* p = dst + static_cast<std::size_t>(gy) * stride;
                    const std::uint8_t bits = mask[gy];
                    if (!bits) {
                        p[0] = kBgColor; p[1] = kBgColor; p[2] = kBgColor; p[3] = kBgColor;
                    } else {
                        p[0] = (bits & 0x80u) ? fg : kBgColor;
                        p[1] = (bits & 0x40u) ? fg : kBgColor;
                        p[2] = (bits & 0x20u) ? fg : kBgColor;
                        p[3] = (bits & 0x10u) ? fg : kBgColor;
                    }
                }
            } else if (gw == 3) {
                for (int gy = 0; gy < gh; ++gy) {
                    std::uint32_t* p = dst + static_cast<std::size_t>(gy) * stride;
                    const std::uint8_t bits = mask[gy];
                    p[0] = (bits & 0x80u) ? fg : kBgColor;
                    p[1] = (bits & 0x40u) ? fg : kBgColor;
                    p[2] = (bits & 0x20u) ? fg : kBgColor;
                }
            } else if (gw == 5) {
                for (int gy = 0; gy < gh; ++gy) {
                    std::uint32_t* p = dst + static_cast<std::size_t>(gy) * stride;
                    const std::uint8_t bits = mask[gy];
                    p[0] = (bits & 0x80u) ? fg : kBgColor;
                    p[1] = (bits & 0x40u) ? fg : kBgColor;
                    p[2] = (bits & 0x20u) ? fg : kBgColor;
                    p[3] = (bits & 0x10u) ? fg : kBgColor;
                    p[4] = (bits & 0x08u) ? fg : kBgColor;
                }
            } else {
                for (int gy = 0; gy < gh; ++gy) {
                    std::uint32_t* p = dst + static_cast<std::size_t>(gy) * stride;
                    std::uint8_t bits = mask[gy];
                    for (int gx = 0; gx < gw; ++gx, bits <<= 1) {
                        p[gx] = (bits & 0x80u) ? fg : kBgColor;
                    }
                }
            }
        };

        for (int y = 0; y < rows; ++y) {
            const Screen::Cell* const srcRow = src + static_cast<std::size_t>(y) * sw;
            std::uint16_t* const prvRow = prevCells_.data() + static_cast<std::size_t>(y) * cols;
            std::uint32_t* const dstBand = dibPixels_ + static_cast<std::size_t>(y * gh) * stride;
            int rowMinX = -1, rowMaxX = -1;
            for (int x = 0; x < cols; ++x) {
                const Screen::Cell cell = srcRow[x];
                const std::uint16_t packed = packCell(cell);
                if (packed == prvRow[x]) continue;
                prvRow[x] = packed;
                if (rowMinX < 0) rowMinX = x;
                rowMaxX = x;
                blitCell(dstBand + x * gw, static_cast<unsigned char>(cell.glyph), cell.color);
            }
            if (rowMinX >= 0) {
                if (y < dirtyMinY) dirtyMinY = y;
                dirtyMaxY = y;
                if (rowMinX < dirtyMinX) dirtyMinX = rowMinX;
                if (rowMaxX > dirtyMaxX) dirtyMaxX = rowMaxX;
            }
        }

        if (dirtyMaxY < dirtyMinY || !winDc_) return;
        const int bltX = full ? 0 : dirtyMinX * gw;
        const int bltY = full ? 0 : dirtyMinY * gh;
        const int bltW = full ? pixelW_ : (dirtyMaxX - dirtyMinX + 1) * gw;
        const int bltH = full ? pixelH_ : (dirtyMaxY - dirtyMinY + 1) * gh;
        BitBlt(winDc_, bltX, bltY, bltW, bltH, memDc_, bltX, bltY, SRCCOPY);
    }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<Win32WindowPlatform*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }
        return self ? self->handleMessage(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
    }

    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_CLOSE:
            quit_ = true;
            PostQuitMessage(0);
            return 0;
        case WM_DESTROY:
            hwnd_ = nullptr;
            winDc_ = nullptr;
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            if (memDc_ && dibPixels_) BitBlt(hdc, 0, 0, pixelW_, pixelH_, memDc_, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_SIZE: {
            const int cw = LOWORD(lp);
            const int ch = HIWORD(lp);
            if (cw >= glyphW_ * 20 && ch >= glyphH_ * 10) resizeDib(cw, ch);
            return 0;
        }
        case WM_KILLFOCUS:
            releaseMouse();
            std::fill_n(held_, 5, false);
            std::fill_n(pressed_, 5, false);
            return 0;
        case WM_LBUTTONDOWN:
            if (!console_) acquireMouse();
            return 0;
        case WM_INPUT:
            if (mouseLocked_) {
                RAWINPUT raw{};
                UINT size = sizeof(raw);
                if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == sizeof(raw) &&
                    raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                    mouseDx_ += raw.data.mouse.lLastX;
                    mouseDy_ += raw.data.mouse.lLastY;
                }
            }
            return 0;
        case WM_SYSKEYDOWN:
            if (wp == VK_RETURN && (lp & (1 << 29))) { toggleFullscreen(); return 0; }
            break;
        case WM_CHAR:
            if (console_) {
                const wchar_t ch = static_cast<wchar_t>(wp);
                if (ch == '`' || ch == '~') return 0;
                if (ch == '\r') consoleKeys_.push_back('\n');
                else if (ch == '\b') consoleKeys_.push_back('\b');
                else if (ch >= 32 && ch <= 126) consoleKeys_.push_back(static_cast<char>(ch));
                return 0;
            }
            return 0;
        case WM_KEYDOWN:
        case WM_KEYUP: {
            const bool down = (msg == WM_KEYDOWN);
            const bool repeat = down && ((lp & (1 << 30)) != 0);
            const UINT vk = static_cast<UINT>(wp);
            if (vk == VK_F11 && down && !repeat) { toggleFullscreen(); return 0; }
            if (vk == VK_OEM_3) {
                if (down && !repeat) {
                    console_ = !console_;
                    consoleToggled_ = true;
                    consoleKeys_.push_back(ConsoleKey::Reset);
                    releaseMouse();
                    std::fill_n(held_, 5, false);
                }
                return 0;
            }
            if (console_) {
                if (!down) return 0;
                if (vk == VK_ESCAPE) { console_ = false; consoleKeys_.push_back(ConsoleKey::Reset); }
                else if (vk == VK_UP) consoleKeys_.push_back(ConsoleKey::Previous);
                else if (vk == VK_DOWN) consoleKeys_.push_back(ConsoleKey::Next);
                return 0;
            }
            int slot = -1;
            switch (vk) {
            case VK_SPACE: slot = 4; break;
            case 'W': slot = 0; if (down && !repeat) menuUp_ = true; break;
            case 'S': slot = 1; if (down && !repeat) menuDown_ = true; break;
            case 'A': slot = 2; break;
            case 'D': slot = 3; break;
            case VK_UP: if (down) menuUp_ = true; return 0;
            case VK_DOWN: if (down) menuDown_ = true; return 0;
            case VK_RETURN: if (down && !repeat) menuConfirm_ = true; return 0;
            case VK_TAB: if (down && !repeat) toggleMinimap_ = true; return 0;
            case 'H': if (down && !repeat) toggleHud_ = true; return 0;
            case 'M': if (down && !repeat) { releaseMouse(); toggleMenu_ = true; } return 0;
            case VK_ESCAPE:
                if (down && !repeat) {
                    if (mouseLocked_) releaseMouse();
                    else toggleMenu_ = true;
                }
                return 0;
            case 'Q': if (down) quit_ = true; return 0;
            default: return 0;
            }
            if (down && !held_[slot]) pressed_[slot] = true;
            held_[slot] = down;
            return 0;
        }
        default: break;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    void resetFrameEvents() {
        mouseDx_ = mouseDy_ = 0;
        consoleToggled_ = false;
        consoleKeys_.clear();
        std::fill_n(pressed_, 5, false);
        toggleHud_ = toggleMinimap_ = toggleMenu_ = false;
        menuUp_ = menuDown_ = menuConfirm_ = false;
    }

    // Bake all 95 printable ASCII glyphs into a compact 1-bit-per-pixel L1-cache table (~600 bytes!).
    void bakeGlyphAtlas(int fontHeight) {
        const int targetH = std::clamp(fontHeight, 4, 24);
        HDC dc = CreateCompatibleDC(nullptr);
        HFONT font = CreateFontW(-targetH, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                 ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        HGDIOBJ oldFont = SelectObject(dc, font);
        TEXTMETRICW tm{};
        GetTextMetricsW(dc, &tm);
        glyphW_ = std::clamp(static_cast<int>(tm.tmAveCharWidth), 3, 8);
        glyphH_ = std::clamp(static_cast<int>(tm.tmHeight), 4, 24);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = glyphW_;
        bmi.bmiHeader.biHeight = -glyphH_;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        std::uint32_t* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&bits), nullptr, 0);
        HGDIOBJ oldBmp = SelectObject(dc, bmp);
        SetBkColor(dc, RGB(0, 0, 0));
        SetTextColor(dc, RGB(255, 255, 255));
        SetBkMode(dc, OPAQUE);

        glyphMasks_.assign(95u * static_cast<std::size_t>(glyphH_), 0);
        RECT rc{0, 0, glyphW_, glyphH_};
        for (int idx = 0; idx < 95; ++idx) {
            std::fill_n(bits, static_cast<std::size_t>(glyphW_) * glyphH_, 0u);
            wchar_t ch = static_cast<wchar_t>(32 + idx);
            ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &rc, &ch, 1, nullptr);
            GdiFlush();
            std::uint8_t* dst = glyphMasks_.data() + static_cast<std::size_t>(idx) * glyphH_;
            for (int y = 0; y < glyphH_; ++y) {
                std::uint8_t rowMask = 0;
                for (int x = 0; x < glyphW_; ++x) {
                    const std::uint32_t lum = bits[y * glyphW_ + x] & 0xFFu;
                    if (lum >= 68u) rowMask |= static_cast<std::uint8_t>(0x80u >> x);
                }
                dst[y] = rowMask;
            }
        }
        SelectObject(dc, oldBmp);
        SelectObject(dc, oldFont);
        DeleteObject(bmp);
        DeleteObject(font);
        DeleteDC(dc);
    }

    void createAppWindow() {
        SetProcessDPIAware();
        HINSTANCE inst = GetModuleHandleW(nullptr);
        WNDCLASSW wc{};
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = wndProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.lpszClassName = L"Raycast2DNativeWindow";
        RegisterClassW(&wc);

        RECT work{0, 0, 1600, 900};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        const int workW = std::max(800, static_cast<int>(work.right - work.left));
        const int workH = std::max(500, static_cast<int>(work.bottom - work.top));
        const int clientW = (static_cast<int>(workW * 0.90) / glyphW_) * glyphW_;
        const int clientH = (static_cast<int>(workH * 0.88) / glyphH_) * glyphH_;

        DWORD style = WS_OVERLAPPEDWINDOW;
        RECT wr{0, 0, clientW, clientH};
        AdjustWindowRect(&wr, style, FALSE);
        const int winW = wr.right - wr.left;
        const int winH = wr.bottom - wr.top;
        const int x = work.left + std::max(0, (workW - winW) / 2);
        const int y = work.top + std::max(0, (workH - winH) / 2);

        hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"RAYCAST-2D  //  R'LYEH: SUNKEN SANCTUM  [F11 Fullscreen]",
                                style, x, y, winW, winH, nullptr, nullptr, inst, this);
        if (!hwnd_) return;
        winDc_ = GetDC(hwnd_);
        resizeDib(clientW, clientH);
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
    }

    void toggleFullscreen() {
        if (!hwnd_) return;
        if (!fullscreen_) {
            savedStyle_ = GetWindowLongW(hwnd_, GWL_STYLE);
            GetWindowRect(hwnd_, &savedRect_);
            HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi{sizeof(MONITORINFO)};
            GetMonitorInfoW(mon, &mi);
            SetWindowLongW(hwnd_, GWL_STYLE, savedStyle_ & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
            fullscreen_ = true;
        } else {
            SetWindowLongW(hwnd_, GWL_STYLE, savedStyle_);
            SetWindowPos(hwnd_, nullptr, savedRect_.left, savedRect_.top,
                         savedRect_.right - savedRect_.left, savedRect_.bottom - savedRect_.top,
                         SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER);
            fullscreen_ = false;
        }
    }

    void resizeDib(int cw, int ch) {
        const int cols = std::max(20, cw / glyphW_);
        const int rows = std::max(10, ch / glyphH_);
        const int pw = cols * glyphW_;
        const int ph = rows * glyphH_;
        if (pw == pixelW_ && ph == pixelH_ && dibPixels_) return;
        destroyDib();

        HDC screenDc = GetDC(hwnd_);
        memDc_ = CreateCompatibleDC(screenDc);
        ReleaseDC(hwnd_, screenDc);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = pw;
        bmi.bmiHeader.biHeight = -ph; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        dibBmp_ = CreateDIBSection(memDc_, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&dibPixels_), nullptr, 0);
        oldDib_ = SelectObject(memDc_, dibBmp_);

        pixelW_ = pw;
        pixelH_ = ph;
        width_ = cols;
        height_ = rows;
        prevCols_ = prevRows_ = 0;
    }

    void destroyDib() {
        if (memDc_) {
            if (oldDib_) SelectObject(memDc_, oldDib_);
            if (dibBmp_) DeleteObject(dibBmp_);
            DeleteDC(memDc_);
        }
        memDc_ = nullptr;
        dibBmp_ = nullptr;
        oldDib_ = nullptr;
        dibPixels_ = nullptr;
    }

    void acquireMouse() {
        if (mouseLocked_ || !hwnd_) return;
        RECT cr{};
        if (!GetClientRect(hwnd_, &cr)) return;
        POINT pt{cr.right / 2, cr.bottom / 2};
        ClientToScreen(hwnd_, &pt);
        GetCursorPos(&savedCursor_);
        RAWINPUTDEVICE dev{1, 2, 0, hwnd_};
        if (!RegisterRawInputDevices(&dev, 1, sizeof(dev))) return;
        RECT pin{pt.x, pt.y, pt.x + 1, pt.y + 1};
        ClipCursor(&pin);
        SetCursorPos(pt.x, pt.y);
        ShowCursor(FALSE);
        mouseLocked_ = true;
    }

    void releaseMouse() {
        if (!mouseLocked_) return;
        RAWINPUTDEVICE dev{1, 2, RIDEV_REMOVE, nullptr};
        RegisterRawInputDevices(&dev, 1, sizeof(dev));
        ClipCursor(nullptr);
        SetCursorPos(savedCursor_.x, savedCursor_.y);
        ShowCursor(TRUE);
        mouseLocked_ = false;
        mouseDx_ = mouseDy_ = 0;
    }

    HWND hwnd_ = nullptr;
    HDC winDc_ = nullptr;
    HDC memDc_ = nullptr;
    HBITMAP dibBmp_ = nullptr;
    HGDIOBJ oldDib_ = nullptr;
    std::uint32_t* dibPixels_ = nullptr;
    int pixelW_ = 0, pixelH_ = 0;
    int glyphW_ = 4, glyphH_ = 6;
    int width_ = 160, height_ = 60;

    std::vector<std::uint8_t> glyphMasks_;
    std::vector<std::uint16_t> prevCells_;
    int prevCols_ = 0, prevRows_ = 0;

    bool fullscreen_ = false;
    LONG savedStyle_ = 0;
    RECT savedRect_{};

    bool mouseLocked_ = false;
    POINT savedCursor_{};
    int mouseDx_ = 0, mouseDy_ = 0;

    bool console_ = false, consoleToggled_ = false;
    std::string consoleKeys_;
    bool held_[5]{};
    bool pressed_[5]{};
    bool quit_ = false;
    bool toggleHud_ = false, toggleMinimap_ = false, toggleMenu_ = false;
    bool menuUp_ = false, menuDown_ = false, menuConfirm_ = false;
};

} // namespace

std::unique_ptr<Platform> createPlatform(int fontHeight) {
    return std::make_unique<Win32WindowPlatform>(fontHeight);
}

#endif
