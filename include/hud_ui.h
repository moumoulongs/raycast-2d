#pragma once

#include "input.h"
#include "player_pose.h"
#include "screen.h"
#include "world_map.h"
#include <filesystem>
#include <string>
#include <vector>

struct RealmEntry {
    std::string label;
    std::string subtitle;
    std::filesystem::path mapPath;
    std::filesystem::path atlasPath;
};

struct HudAction {
    bool quit = false;
    int switchRealmIndex = -1; // >= 0 requests hot-swapping to that realm
};

class HudUi {
public:
    enum class Mode { Playing, Title, Paused, Realms, Controls };

    HudUi(std::vector<RealmEntry> realms, int activeRealm);

    bool blocksMovement() const { return mode_ != Mode::Playing; }
    Mode mode() const { return mode_; }

    HudAction update(const InputState& input, double dt);
    void draw(Screen& screen, const WorldMap& map, const PlayerPose& pose,
              const std::string& mapName, double sceneTime, double fps,
              bool mouseLocked, bool netConnected) const;

private:
    struct UiScale {
        bool bitmap = false;
        int pxW = 1, pxH = 1;       // cells per bitmap pixel
        int charAdv = 1, lineAdv = 1; // cells per character step & line step
        int padX = 1, padY = 1;     // inner box border thickness
        int mapCellX = 1, mapCellY = 1;
    };

    static UiScale computeScale(int sw, int sh);
    void fillRect(Screen& screen, int x0, int y0, int w, int h, char g, unsigned char col) const;
    void drawBox(Screen& screen, int x0, int y0, int w, int h, int bx, int by, unsigned char borderCol, unsigned char fillCol) const;
    void drawTextScaled(Screen& screen, int x, int y, const std::string& text, unsigned char color,
                        int pxW, int pxH, bool bitmap, int maxChars = -1) const;
    void drawHud(Screen& screen, const WorldMap& map, const PlayerPose& pose,
                 const std::string& mapName, double sceneTime, double fps,
                 bool mouseLocked, bool netConnected, const UiScale& s) const;
    void drawCompass(Screen& screen, const WorldMap& map, const PlayerPose& pose, double sceneTime, const UiScale& s) const;
    void drawMinimap(Screen& screen, const WorldMap& map, const PlayerPose& pose, const UiScale& s) const;
    void drawOverlay(Screen& screen, double sceneTime, const UiScale& s) const;

    std::vector<RealmEntry> realms_;
    int activeRealm_ = 0;
    Mode mode_ = Mode::Title;
    Mode returnMode_ = Mode::Title;
    int cursor_ = 0;
    bool showHud_ = true;
    bool showMinimap_ = true;
    double sanity_ = 100.0;
};
