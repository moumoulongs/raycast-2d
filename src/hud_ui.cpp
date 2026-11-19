#include "hud_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

double wrapPi(double a) {
    while (a > kPi) a -= kTwoPi;
    while (a < -kPi) a += kTwoPi;
    return a;
}

std::string upperTitle(const std::string& filename) {
    std::string out;
    for (char c : filename) {
        if (c == '.') break;
        if (c == '_' || c == '-') out.push_back(' ');
        else if (c >= 'a' && c <= 'z') out.push_back(static_cast<char>(c - 'a' + 'A'));
        else out.push_back(c);
    }
    return out.empty() ? "UNKNOWN" : out;
}

// Compact 2-byte (15-bit) 3x5 bitmap font for sharp integer-scaled pixel HUD typography.
constexpr std::uint16_t G(int r0, int r1, int r2, int r3, int r4) {
    return static_cast<std::uint16_t>((r0 << 12) | (r1 << 9) | (r2 << 6) | (r3 << 3) | r4);
}

std::uint16_t glyphBits(char raw) {
    char c = (raw >= 'a' && raw <= 'z') ? static_cast<char>(raw - 'a' + 'A') : raw;
    switch (c) {
    case 'A': return G(2,5,7,5,5);
    case 'B': return G(6,5,6,5,6);
    case 'C': return G(3,4,4,4,3);
    case 'D': return G(6,5,5,5,6);
    case 'E': return G(7,4,6,4,7);
    case 'F': return G(7,4,6,4,4);
    case 'G': return G(3,4,5,5,3);
    case 'H': return G(5,5,7,5,5);
    case 'I': return G(7,2,2,2,7);
    case 'J': return G(7,1,1,5,2);
    case 'K': return G(5,5,6,5,5);
    case 'L': return G(4,4,4,4,7);
    case 'M': return G(5,7,7,5,5);
    case 'N': return G(6,5,5,5,5);
    case 'O': return G(2,5,5,5,2);
    case 'P': return G(6,5,6,4,4);
    case 'Q': return G(2,5,5,7,3);
    case 'R': return G(6,5,6,5,5);
    case 'S': return G(3,4,2,1,6);
    case 'T': return G(7,2,2,2,2);
    case 'U': return G(5,5,5,5,7);
    case 'V': return G(5,5,5,5,2);
    case 'W': return G(5,5,7,7,5);
    case 'X': return G(5,5,2,5,5);
    case 'Y': return G(5,5,2,2,2);
    case 'Z': return G(7,1,2,4,7);
    case '0': return G(7,5,5,5,7);
    case '1': return G(2,6,2,2,7);
    case '2': return G(7,1,7,4,7);
    case '3': return G(7,1,7,1,7);
    case '4': return G(5,5,7,1,1);
    case '5': return G(7,4,7,1,7);
    case '6': return G(7,4,7,5,7);
    case '7': return G(7,1,2,2,2);
    case '8': return G(7,5,7,5,7);
    case '9': return G(7,5,7,1,7);
    case ':': return G(0,2,0,2,0);
    case '.': return G(0,0,0,0,2);
    case ',': return G(0,0,0,2,4);
    case '-': return G(0,0,7,0,0);
    case '+': return G(0,2,7,2,0);
    case '=': return G(0,7,0,7,0);
    case '/': return G(1,1,2,4,4);
    case '\\':return G(4,4,2,1,1);
    case '[': return G(6,4,4,4,6);
    case ']': return G(3,1,1,1,3);
    case '(': return G(2,4,4,4,2);
    case ')': return G(2,1,1,1,2);
    case '<': return G(1,2,4,2,1);
    case '>': return G(4,2,1,2,4);
    case '*': return G(0,5,2,5,0);
    case '!': return G(2,2,2,0,2);
    case '?': return G(7,1,2,0,2);
    case '\'':return G(2,2,0,0,0);
    case '~': return G(0,3,6,0,0);
    case '^': return G(2,5,0,0,0);
    case '_': return G(0,0,0,0,7);
    case '|': return G(2,2,2,2,2);
    case '#': return G(7,7,7,7,7);
    case '&': return G(2,5,2,5,3);
    default:  return 0;
    }
}
} // namespace

HudUi::UiScale HudUi::computeScale(int sw, int sh) {
    UiScale s{};
    if (sw < 176 || sh < 62) {
        // Tier 1: Classic 1x terminal typography
        s.bitmap = false;
        s.pxW = 1; s.pxH = 1;
        s.charAdv = 1; s.lineAdv = 1;
        s.padX = 1; s.padY = 1;
        s.mapCellX = 1; s.mapCellY = 1;
        return s;
    }
    s.bitmap = true;
    if (sw < 320 || sh < 108) {
        // Tier 2: Crisp 3x5 pixel font (4x6 cell pitch)
        s.pxW = 1; s.pxH = 1;
        s.charAdv = 4; s.lineAdv = 6;
        s.padX = 2; s.padY = 1;
        s.mapCellX = 2; s.mapCellY = 1;
    } else if (sw < 520 || sh < 172) {
        // Tier 3: Bold square-pixel 6x5 font (8x7 cell pitch)
        s.pxW = 2; s.pxH = 1;
        s.charAdv = 8; s.lineAdv = 7;
        s.padX = 3; s.padY = 2;
        s.mapCellX = 3; s.mapCellY = 2;
    } else {
        // Tier 4: Giant high-DPI 9x10 font (12x12 cell pitch)
        s.pxW = 3; s.pxH = 2;
        s.charAdv = 12; s.lineAdv = 12;
        s.padX = 4; s.padY = 2;
        s.mapCellX = 4; s.mapCellY = 2;
    }
    return s;
}

HudUi::HudUi(std::vector<RealmEntry> realms, int activeRealm)
    : realms_(std::move(realms)), activeRealm_(activeRealm) {}

HudAction HudUi::update(const InputState& input, double dt) {
    HudAction act{};
    if (input.consoleActive) return act;

    if (input.toggleHud && mode_ == Mode::Playing) showHud_ = !showHud_;
    if (input.toggleMinimap && mode_ == Mode::Playing) showMinimap_ = !showMinimap_;

    if (input.toggleMenu) {
        if (mode_ == Mode::Playing) {
            mode_ = Mode::Paused;
            cursor_ = 0;
        } else if (mode_ == Mode::Realms || mode_ == Mode::Controls) {
            mode_ = returnMode_;
            cursor_ = 0;
        } else if (mode_ == Mode::Paused || mode_ == Mode::Title) {
            mode_ = Mode::Playing;
        }
        return act;
    }

    if (mode_ == Mode::Playing) {
        sanity_ = std::min(100.0, sanity_ + dt * 2.0);
        return act;
    }

    const int itemCount = (mode_ == Mode::Realms)
        ? static_cast<int>(realms_.size()) + 1
        : (mode_ == Mode::Controls ? 1 : 4);

    if (input.menuUp) cursor_ = (cursor_ + itemCount - 1) % itemCount;
    if (input.menuDown) cursor_ = (cursor_ + 1) % itemCount;

    if (mode_ == Mode::Title && (input.jump || input.mouseLocked)) {
        mode_ = Mode::Playing;
        return act;
    }
    if (mode_ == Mode::Paused && input.mouseLocked) {
        mode_ = Mode::Playing;
        return act;
    }

    if (input.menuConfirm) {
        if (mode_ == Mode::Title || mode_ == Mode::Paused) {
            if (cursor_ == 0) {
                mode_ = Mode::Playing;
            } else if (cursor_ == 1) {
                returnMode_ = mode_;
                mode_ = Mode::Realms;
                cursor_ = std::clamp(activeRealm_, 0, std::max(0, static_cast<int>(realms_.size()) - 1));
            } else if (cursor_ == 2) {
                returnMode_ = mode_;
                mode_ = Mode::Controls;
                cursor_ = 0;
            } else if (cursor_ == 3) {
                act.quit = true;
            }
        } else if (mode_ == Mode::Realms) {
            if (cursor_ < static_cast<int>(realms_.size())) {
                activeRealm_ = cursor_;
                act.switchRealmIndex = cursor_;
                mode_ = Mode::Playing;
            } else {
                mode_ = returnMode_;
                cursor_ = 0;
            }
        } else if (mode_ == Mode::Controls) {
            mode_ = returnMode_;
            cursor_ = 0;
        }
    }
    return act;
}

void HudUi::fillRect(Screen& screen, int x0, int y0, int w, int h, char g, unsigned char col) const {
    const int x1 = std::min(screen.width(), x0 + w);
    const int y1 = std::min(screen.height(), y0 + h);
    const int sx = std::max(0, x0);
    const int sy = std::max(0, y0);
    for (int y = sy; y < y1; ++y) {
        for (int x = sx; x < x1; ++x) {
            screen.drawCellUnchecked(x, y, g, col);
        }
    }
}

void HudUi::drawBox(Screen& screen, int x0, int y0, int w, int h, int bx, int by,
                    unsigned char borderCol, unsigned char fillCol) const {
    if (w < 2 || h < 2) return;
    fillRect(screen, x0, y0, w, h, ' ', fillCol);
    // Horizontal top/bottom border bands
    fillRect(screen, x0, y0, w, by, '=', borderCol);
    fillRect(screen, x0, y0 + h - by, w, by, '=', borderCol);
    // Vertical left/right border bands
    fillRect(screen, x0, y0, bx, h, '|', borderCol);
    fillRect(screen, x0 + w - bx, y0, bx, h, '|', borderCol);
    // Corners
    fillRect(screen, x0, y0, bx, by, '+', borderCol);
    fillRect(screen, x0 + w - bx, y0, bx, by, '+', borderCol);
    fillRect(screen, x0, y0 + h - by, bx, by, '+', borderCol);
    fillRect(screen, x0 + w - bx, y0 + h - by, bx, by, '+', borderCol);
}

void HudUi::drawTextScaled(Screen& screen, int x, int y, const std::string& text, unsigned char color,
                           int pxW, int pxH, bool bitmap, int maxChars) const {
    const int count = maxChars >= 0 ? std::min(maxChars, static_cast<int>(text.size())) : static_cast<int>(text.size());
    if (!bitmap) {
        for (int i = 0; i < count; ++i) {
            screen.drawCell(x + i, y, text[static_cast<size_t>(i)], color);
        }
        return;
    }
    const int adv = 4 * pxW;
    for (int i = 0; i < count; ++i) {
        const std::uint16_t bits = glyphBits(text[static_cast<size_t>(i)]);
        if (!bits) continue;
        const int gx = x + i * adv;
        for (int row = 0; row < 5; ++row) {
            const int rowBits = (bits >> ((4 - row) * 3)) & 7;
            if (!rowBits) continue;
            for (int col = 0; col < 3; ++col) {
                if (rowBits & (4 >> col)) {
                    fillRect(screen, gx + col * pxW, y + row * pxH, pxW, pxH, '#', color);
                }
            }
        }
    }
}

void HudUi::drawCompass(Screen& screen, const WorldMap& map, const PlayerPose& pose, double sceneTime, const UiScale& s) const {
    const int sw = screen.width();
    const int cw = s.bitmap ? std::clamp(sw * 38 / 100, 36 * s.pxW, sw - 2) : std::min(46, sw - 46);
    if (cw < 20) return;
    const int ch = s.bitmap ? (5 * s.pxH + s.padY * 2 + 2 * s.pxH) : 3;
    const int cx0 = (sw - cw) / 2;
    drawBox(screen, cx0, 0, cw, ch, s.padX, s.padY, 6, 0);

    const double targetX = map.gridWidth() * 0.5;
    const double targetY = map.gridHeight() * 0.43;
    const double toTarget = std::atan2(targetY - pose.y, targetX - pose.x);
    const double fovSpan = 1.65;

    const int innerX = cx0 + s.padX + s.pxW;
    const int innerW = cw - 2 * (s.padX + s.pxW);
    const int midY = ch / 2;
    if (innerW < 8) return;

    if (!s.bitmap) {
        for (int i = 0; i < innerW; ++i) {
            const double frac = (static_cast<double>(i) / std::max(1, innerW - 1)) - 0.5;
            const double ang = wrapPi(pose.yaw + frac * fovSpan);
            const double ticks = (ang + kPi) / kTwoPi * 32.0;
            const int nearest = static_cast<int>(std::round(ticks));
            const double err = std::abs(ticks - nearest);
            char g = '-';
            unsigned char col = 8;
            if (err < 0.22) {
                const int dir = ((nearest % 32) + 32) % 32;
                if (dir == 8) { g = 'N'; col = 14; }
                else if (dir == 16) { g = 'E'; col = 6; }
                else if (dir == 24) { g = 'S'; col = 6; }
                else if (dir == 0) { g = 'W'; col = 6; }
                else if (dir % 4 == 0) { g = '+'; col = 7; }
                else if (dir % 2 == 0) { g = ':'; col = 8; }
            }
            screen.drawCell(innerX + i, 1, g, col);
        }
        screen.drawCell(cx0 + cw / 2, 0, 'v', 15);
        screen.drawCell(cx0 + cw / 2, 2, '^', 15);
        const double rel = wrapPi(toTarget - pose.yaw);
        if (std::abs(rel) < fovSpan * 0.48) {
            const int bx = innerX + static_cast<int>((rel / fovSpan + 0.5) * (innerW - 1));
            screen.drawCell(bx, 1, '*', std::sin(sceneTime * 4.0) > 0.0 ? 9 : 13);
        }
        return;
    }

    // Scaled high-resolution compass bar!
    fillRect(screen, innerX, midY, innerW, s.pxH, '-', 8);
    // Draw cardinal & intercardinal scaled markers cleanly at their exact angular projection
    struct DirMark { double ang; const char* label; unsigned char col; };
    static const DirMark kDirs[] = {
        {-kPi * 0.5, "N", 14}, {0.0, "E", 6}, {kPi * 0.5, "S", 6}, {kPi, "W", 6},
        {-kPi * 0.25, "NE", 7}, {kPi * 0.25, "SE", 7}, {kPi * 0.75, "SW", 7}, {-kPi * 0.75, "NW", 7}
    };
    const int labelY = (ch - 5 * s.pxH) / 2;
    for (const auto& d : kDirs) {
        const double rel = wrapPi(d.ang - pose.yaw);
        if (std::abs(rel) > fovSpan * 0.46) continue;
        const int len = static_cast<int>(std::char_traits<char>::length(d.label));
        const int px = innerX + static_cast<int>((rel / fovSpan + 0.5) * innerW) - (len * s.charAdv) / 2;
        if (px >= innerX && px + len * s.charAdv <= innerX + innerW) {
            drawTextScaled(screen, px, labelY, d.label, d.col, s.pxW, s.pxH, true);
        }
    }
    // Center sightline chevron
    fillRect(screen, cx0 + cw / 2 - s.pxW / 2, 0, s.pxW, s.padY + s.pxH, 'V', 15);
    fillRect(screen, cx0 + cw / 2 - s.pxW / 2, ch - s.padY - s.pxH, s.pxW, s.padY + s.pxH, '^', 15);

    // Pulsing crimson sanctum beacon
    const double rel = wrapPi(toTarget - pose.yaw);
    if (std::abs(rel) < fovSpan * 0.46) {
        const int bx = innerX + static_cast<int>((rel / fovSpan + 0.5) * innerW);
        const unsigned char bCol = std::sin(sceneTime * 4.0) > 0.0 ? 9 : 13;
        fillRect(screen, bx - s.pxW, labelY + s.pxH, s.pxW * 2, 3 * s.pxH, '@', bCol);
    }
}

void HudUi::drawMinimap(Screen& screen, const WorldMap& map, const PlayerPose& pose, const UiScale& s) const {
    const int sw = screen.width();
    const int sh = screen.height();
    if (sw < 64 || sh < 20) return;

    const int radX = s.bitmap ? 9 : ((sw >= 150) ? 9 : 7);
    const int radY = s.bitmap ? 6 : ((sh >= 48) ? 5 : 4);
    const int gridCellsX = (radX * 2 + 1) * s.mapCellX;
    const int gridCellsY = (radY * 2 + 1) * s.mapCellY;
    const int headerH = s.bitmap ? (5 * s.pxH + s.pxH) : 0;
    const int mw = gridCellsX + s.padX * 2 + (s.bitmap ? 2 : 0);
    const int mh = gridCellsY + s.padY * 2 + headerH + (s.bitmap ? 1 : 0);
    if (mw > sw / 3 || mh > sh / 2) return;

    const int mx0 = sw - mw - s.padX;
    const int my0 = 0;
    drawBox(screen, mx0, my0, mw, mh, s.padX, s.padY, 6, 0);

    if (s.bitmap) {
        drawTextScaled(screen, mx0 + s.padX + s.pxW, my0 + s.padY + 1, "RADAR", 14, s.pxW, s.pxH, true);
    } else {
        drawTextScaled(screen, mx0 + 2, my0, " RADAR [TAB] ", 14, 1, 1, false, mw - 4);
    }

    const int originX = mx0 + s.padX + (s.bitmap ? 1 : 0);
    const int originY = my0 + s.padY + headerH + (s.bitmap ? 1 : 0);
    const int px = static_cast<int>(pose.x);
    const int py = static_cast<int>(pose.y);

    for (int dy = -radY; dy <= radY; ++dy) {
        for (int dx = -radX; dx <= radX; ++dx) {
            const int gx = px + dx;
            const int gy = py + dy;
            char g = ' ';
            unsigned char c = 8;
            if (map.contains(gx, gy)) {
                const auto& t = map.at(gx, gy);
                if (t.wall) {
                    if (t.wallHeight > 3.6f) { g = 'M'; c = 13; }
                    else if (t.wallHeight > 2.2f) { g = '#'; c = 6; }
                    else { g = '#'; c = 8; }
                } else if (t.floor < -1.8) {
                    g = '~'; c = 4;
                } else if (t.floor > 0.8) {
                    g = '^'; c = 11;
                } else if (t.floor > 0.12) {
                    g = '='; c = 14;
                } else if (t.floor < -0.15) {
                    g = '-'; c = 6;
                } else {
                    g = '.'; c = 8;
                }
            }
            fillRect(screen, originX + (dx + radX) * s.mapCellX, originY + (dy + radY) * s.mapCellY,
                     s.mapCellX, s.mapCellY, g, c);
        }
    }

    for (const auto& prop : map.props()) {
        const int dx = static_cast<int>(prop.x) - px;
        const int dy = static_cast<int>(prop.y) - py;
        if (std::abs(dx) <= radX && std::abs(dy) <= radY) {
            fillRect(screen, originX + (dx + radX) * s.mapCellX, originY + (dy + radY) * s.mapCellY,
                     s.mapCellX, s.mapCellY, '*', prop.width > 4.0 ? 9 : 10);
        }
    }

    const double ca = std::cos(pose.yaw), sa = std::sin(pose.yaw);
    char arrow = '@';
    if (std::abs(ca) > std::abs(sa)) arrow = ca > 0 ? '>' : '<';
    else arrow = sa > 0 ? 'v' : '^';
    fillRect(screen, originX + radX * s.mapCellX, originY + radY * s.mapCellY,
             s.mapCellX, s.mapCellY, arrow, 15);
}

void HudUi::drawHud(Screen& screen, const WorldMap& map, const PlayerPose& pose,
                    const std::string& mapName, double sceneTime, double fps,
                    bool mouseLocked, bool netConnected, const UiScale& s) const {
    const int sw = screen.width();
    const int sh = screen.height();

    // 1. Top-Left Scaled Telemetry & Sanity Box
    if (sw >= 50 && sh >= 14) {
        const int maxChars = 16;
        const int bw = s.bitmap
            ? std::min(sw * 28 / 100, maxChars * s.charAdv + s.padX * 2 + s.pxW * 2)
            : std::min(30, sw / 3);
        const int bh = s.bitmap
            ? (4 * s.lineAdv + s.padY * 2 + 2 * s.pxH)
            : 5;
        drawBox(screen, s.padX, 0, bw, bh, s.padX, s.padY, 6, 0);

        const int tx = s.padX * 2 + (s.bitmap ? s.pxW : 1);
        const int usableChars = std::max(6, (bw - s.padX * 2 - (s.bitmap ? s.pxW * 2 : 2)) / s.charAdv);
        const std::string title = upperTitle(mapName);

        const double lookNorth = std::max(0.0, -std::sin(pose.yaw));
        const double dread = map.sky().eldritch
            ? (18.0 * lookNorth + 10.0 * (0.5 + 0.5 * std::sin(sceneTime * 1.3)) + (pose.feet > 0.7 ? 12.0 : 0.0))
            : 4.0;
        const int sanPct = static_cast<int>(std::clamp(sanity_ - dread, 18.0, 100.0));
        const unsigned char sanCol = sanPct < 65 ? 9 : (sanPct < 82 ? 13 : 10);

        char fpsLine[64];
        std::snprintf(fpsLine, sizeof(fpsLine), "FPS %4.0f %s", std::clamp(fps, 0.0, 9999.0), netConnected ? "NET" : "SOLO");
        char posLine[64];
        std::snprintf(posLine, sizeof(posLine), "%4.1f,%4.1f Z%+3.1f", pose.x, pose.y, pose.feet);

        if (!s.bitmap) {
            drawTextScaled(screen, 3, 0, " " + title + " ", map.sky().eldritch ? 10 : 14, 1, 1, false, bw - 4);
            drawTextScaled(screen, 3, 1, fpsLine, 14, 1, 1, false, bw - 4);
            const int barW = std::max(6, bw - 14);
            const int filled = (sanPct * barW + 50) / 100;
            std::string bar = "SAN [";
            for (int i = 0; i < barW; ++i) bar.push_back(i < filled ? '#' : '.');
            bar += "]";
            drawTextScaled(screen, 3, 2, bar, sanCol, 1, 1, false, bw - 4);
            drawTextScaled(screen, 3, 3, posLine, 7, 1, 1, false, bw - 4);
        } else {
            const int ty0 = s.padY + s.pxH;
            drawTextScaled(screen, tx, ty0, title, map.sky().eldritch ? 10 : 14, s.pxW, s.pxH, true, usableChars);
            drawTextScaled(screen, tx, ty0 + s.lineAdv, fpsLine, 14, s.pxW, s.pxH, true, usableChars);
            // Bold scaled SAN label + solid graphical sanity meter bar!
            drawTextScaled(screen, tx, ty0 + 2 * s.lineAdv, "SAN", sanCol, s.pxW, s.pxH, true, 3);
            const int barX = tx + 4 * s.charAdv;
            const int barMaxW = std::max(4, (s.padX + bw - s.padX - s.pxW) - barX);
            const int barH = 3 * s.pxH;
            const int barY = ty0 + 2 * s.lineAdv + s.pxH;
            const int fillW = (sanPct * barMaxW + 50) / 100;
            fillRect(screen, barX, barY, barMaxW, barH, ':', 8);
            fillRect(screen, barX, barY, fillW, barH, '#', sanCol);
            drawTextScaled(screen, tx, ty0 + 3 * s.lineAdv, posLine, 7, s.pxW, s.pxH, true, usableChars);
        }
    }

    // 2. Top-Center 360° Dynamic Compass
    drawCompass(screen, map, pose, sceneTime, s);

    // 3. Top-Right Live ASCII Radar Minimap
    if (showMinimap_) drawMinimap(screen, map, pose, s);

    // 4. Window-Scaled Center Crosshair
    if (mode_ == Mode::Playing) {
        const int cx = sw / 2, cy = sh / 2;
        const unsigned char col = mouseLocked ? 14 : 8;
        if (!s.bitmap) {
            screen.drawCell(cx, cy, '+', col);
        } else {
            const int armX = 2 * s.pxW + 1;
            const int armY = s.pxH + 1;
            fillRect(screen, cx - armX, cy, armX - s.pxW + 1, s.pxH, '-', col);
            fillRect(screen, cx + s.pxW, cy, armX - s.pxW + 1, s.pxH, '-', col);
            fillRect(screen, cx, cy - armY, s.pxW, armY - s.pxH + 1, '|', col);
            fillRect(screen, cx, cy + s.pxH, s.pxW, armY - s.pxH + 1, '|', col);
            fillRect(screen, cx, cy, s.pxW, s.pxH, '+', 15);
        }
    }

    // 5. Bottom Window-Scaled Status & Control Bar
    if (sh >= 8) {
        const int barH = s.bitmap ? (5 * s.pxH + 2 * s.pxH) : 1;
        const int by = sh - barH;
        fillRect(screen, 0, by, sw, barH, ' ', 0);
        if (s.bitmap) fillRect(screen, 0, by, sw, 1, '=', 6);
        const int textY = by + (s.bitmap ? s.pxH + 1 : 0);

        const std::string mouseTag = mouseLocked ? "[LOCKED]" : "[CLICK LOOK]";
        const int tagChars = static_cast<int>(mouseTag.size());
        drawTextScaled(screen, s.padX, textY, mouseTag, mouseLocked ? 10 : 11, s.pxW, s.pxH, s.bitmap);

        const std::string hints = s.bitmap
            ? "WASD MOVE  SPC JUMP  TAB MAP  H HUD  M REALM  ESC MENU  Q QUIT"
            : "[WASD] Move  [SPACE] Jump  [TAB] Radar  [H] HUD  [M] Realms  [~] Console  [ESC] Menu  [Q] Quit";
        const int hintX = s.padX + (tagChars + 2) * s.charAdv;
        if (hintX + 4 * s.charAdv < sw) {
            const int availChars = (sw - hintX - s.padX) / s.charAdv;
            drawTextScaled(screen, hintX, textY, hints, 7, s.pxW, s.pxH, s.bitmap, availChars);
        }
    }
}

void HudUi::drawOverlay(Screen& screen, double sceneTime, const UiScale& s) const {
    const int sw = screen.width();
    const int sh = screen.height();
    const bool pulse = std::sin(sceneTime * 3.0) > 0.0;

    // Scale overlay panel proportionally to window size!
    const int bw = s.bitmap ? std::clamp(sw * 70 / 100, 32 * s.charAdv, sw - 4) : std::min(62, sw - 4);
    const int bh = s.bitmap ? std::clamp(sh * 74 / 100, 10 * s.lineAdv, sh - 2) : std::min((mode_ == Mode::Title ? 21 : 18), sh - 2);
    if (bw < 30 || bh < 11) return;
    const int bx = (sw - bw) / 2;
    const int by = (sh - bh) / 2;

    drawBox(screen, bx, by, bw, bh, s.padX, s.padY, 13, 0);
    // Pulsing corner eldritch sigils
    fillRect(screen, bx + s.padX, by, s.pxW, s.padY, '*', pulse ? 10 : 14);
    fillRect(screen, bx + bw - s.padX - s.pxW, by, s.pxW, s.padY, '*', pulse ? 10 : 14);
    fillRect(screen, bx + s.padX, by + bh - s.padY, s.pxW, s.padY, '*', pulse ? 9 : 13);
    fillRect(screen, bx + bw - s.padX - s.pxW, by + bh - s.padY, s.pxW, s.padY, '*', pulse ? 9 : 13);

    const int innerChars = std::max(8, (bw - s.padX * 2 - 2 * s.pxW) / s.charAdv);
    auto centerText = [&](int y, const std::string& text, unsigned char col, int multW = 1, int multH = 1) {
        const int pw = s.pxW * multW;
        const int ph = s.pxH * multH;
        const int adv = s.bitmap ? (4 * pw) : 1;
        const int maxC = std::max(1, (bw - s.padX * 2 - 2 * pw) / adv);
        const int len = std::min(static_cast<int>(text.size()), maxC);
        const int pixelW = s.bitmap ? (len * adv - pw) : len;
        const int tx = bx + (bw - pixelW) / 2;
        if (y <= by || y + (s.bitmap ? 5 * ph : 1) >= by + bh) return;
        drawTextScaled(screen, tx, y, text, col, pw, ph, s.bitmap, len);
    };

    const int stepY = s.bitmap ? (s.lineAdv + std::max(1, s.pxH)) : 2;

    if (mode_ == Mode::Title) {
        if (!s.bitmap && bh >= 18) {
            centerText(by + 2, " ____  _ _   _   _ _____ _   _ ", 10);
            centerText(by + 3, "|  _ \\| ( ) | | | | ____| | | |", 14);
            centerText(by + 4, "| |_) | |/  | |_| |  _| | |_| |", 14);
            centerText(by + 5, "|  _ <| |   |  _  | |___|  _  |", 6);
            centerText(by + 6, "|_| \\_\\_|   |_| |_|_____|_| |_|", 5);
            centerText(by + 8, "~ SUNKEN SANCTUM OF THE SLEEPER ~", pulse ? 9 : 13);
        } else if (s.bitmap) {
            // Giant 2x-scaled bold pixel banner for large windows!
            const int titleH = 5 * (s.pxH * 2);
            centerText(by + s.padY + s.pxH * 2, "R'LYEH", 10, 2, 2);
            centerText(by + s.padY + s.pxH * 3 + titleH, "~ SUNKEN SANCTUM OF THE SLEEPER ~", pulse ? 9 : 13);
        } else {
            centerText(by + 2, "=== R'LYEH : SUNKEN SANCTUM ===", 10);
        }

        const int menuY = s.bitmap
            ? (by + s.padY + s.pxH * 5 + 5 * (s.pxH * 2) + s.lineAdv)
            : (by + (bh >= 18 ? 11 : 5));
        static const char* kItems[] = {
            "ENTER THE ABYSS",
            "SELECT REALM",
            "CONTROLS & GUIDE",
            "QUIT TO DESKTOP"
        };
        for (int i = 0; i < 4; ++i) {
            const bool sel = (cursor_ == i);
            std::string row = sel ? (std::string(">> [ ") + kItems[i] + " ] <<") : (std::string("   ") + kItems[i] + "   ");
            centerText(menuY + i * stepY, row, sel ? 11 : 7);
        }
        centerText(by + bh - s.padY - (s.bitmap ? 5 * s.pxH + s.pxH : 2), "W/S SELECT  ENTER/SPACE CONFIRM", 8);
    } else if (mode_ == Mode::Paused) {
        centerText(by + s.padY + s.pxH * 2, "SANCTUM PAUSED", 14, s.bitmap ? 2 : 1, s.bitmap ? 2 : 1);
        const int subY = by + s.padY + s.pxH * 3 + (s.bitmap ? 10 * s.pxH : 2);
        if (activeRealm_ >= 0 && activeRealm_ < static_cast<int>(realms_.size())) {
            centerText(subY, "REALM: " + realms_[activeRealm_].label, 10);
        }
        const int menuY = subY + stepY + (s.bitmap ? s.pxH : 1);
        static const char* kItems[] = {
            "RESUME EXPLORATION",
            "SELECT REALM",
            "CONTROLS & GUIDE",
            "QUIT TO DESKTOP"
        };
        for (int i = 0; i < 4; ++i) {
            const bool sel = (cursor_ == i);
            std::string row = sel ? (std::string(">> [ ") + kItems[i] + " ] <<") : (std::string("   ") + kItems[i] + "   ");
            centerText(menuY + i * stepY, row, sel ? 11 : 7);
        }
        centerText(by + bh - s.padY - (s.bitmap ? 5 * s.pxH + s.pxH : 2), "ESC RESUME   ENTER CONFIRM", 8);
    } else if (mode_ == Mode::Realms) {
        centerText(by + s.padY + s.pxH * 2, "SELECT REALM", 14, s.bitmap ? 2 : 1, s.bitmap ? 2 : 1);
        const int listY = by + s.padY + s.pxH * 4 + (s.bitmap ? 10 * s.pxH : 2);
        const int itemStep = s.bitmap ? s.lineAdv : 1;
        const int footerY = by + bh - s.padY - (s.bitmap ? 5 * s.pxH + s.pxH : 2);
        const int backY = footerY - stepY;
        const int listX = bx + s.padX + 2 * s.pxW;
        for (int i = 0; i < static_cast<int>(realms_.size()); ++i) {
            const int ry = listY + i * itemStep;
            if (ry + (s.bitmap ? 5 * s.pxH : 1) >= backY) break;
            const bool sel = (cursor_ == i);
            const bool cur = (activeRealm_ == i);
            std::string label = (sel ? ">> " : "   ") + realms_[i].label;
            if (!s.bitmap && !realms_[i].subtitle.empty()) label += "  - " + realms_[i].subtitle;
            if (cur) label += " [ON]";
            drawTextScaled(screen, listX, ry, label, sel ? 11 : (cur ? 10 : 7), s.pxW, s.pxH, s.bitmap, innerChars);
        }
        const bool backSel = (cursor_ == static_cast<int>(realms_.size()));
        centerText(backY, backSel ? ">> [ BACK ] <<" : "   BACK   ", backSel ? 11 : 8);
        centerText(footerY, "ENTER LOAD REALM   ESC BACK", 8);
    } else if (mode_ == Mode::Controls) {
        centerText(by + s.padY + s.pxH * 2, "CONTROLS", 14, s.bitmap ? 2 : 1, s.bitmap ? 2 : 1);
        static const char* kLines[] = {
            "WASD       MOVE & STRAFE",
            "SPACE      JUMP / VAULT",
            "CLICK      LOCK MOUSE LOOK",
            "ESC        UNLOCK / MENU",
            "TAB        TOGGLE RADAR",
            "H          TOGGLE HUD",
            "M          SWITCH REALM",
            "~          NET CONSOLE",
            "Q          QUIT GAME"
        };
        const int listY = by + s.padY + s.pxH * 4 + (s.bitmap ? 10 * s.pxH : 2);
        const int itemStep = s.bitmap ? s.lineAdv : 1;
        const int footerY = by + bh - s.padY - (s.bitmap ? 5 * s.pxH + s.pxH : 2);
        const int listX = bx + s.padX + 3 * s.pxW;
        for (int i = 0; i < 9; ++i) {
            const int ry = listY + i * itemStep;
            if (ry + (s.bitmap ? 5 * s.pxH : 1) >= footerY) break;
            drawTextScaled(screen, listX, ry, kLines[i], i < 4 ? 10 : 7, s.pxW, s.pxH, s.bitmap, innerChars - 2);
        }
        centerText(footerY, ">> ENTER OR ESC TO RETURN <<", 11);
    }
}

void HudUi::draw(Screen& screen, const WorldMap& map, const PlayerPose& pose,
                 const std::string& mapName, double sceneTime, double fps,
                 bool mouseLocked, bool netConnected) const {
    const UiScale s = computeScale(screen.width(), screen.height());
    if (mode_ == Mode::Playing && showHud_) {
        drawHud(screen, map, pose, mapName, sceneTime, fps, mouseLocked, netConnected, s);
    } else if (mode_ != Mode::Playing) {
        drawOverlay(screen, sceneTime, s);
    }
}
