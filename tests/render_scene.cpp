#include "camera.h"
#include "hud_ui.h"
#include <iostream>

int main(int argc, char** argv) {
    WorldMap rlyeh("assets/maps/rlyeh.map", "assets/textures/rlyeh.atlas");
    if (!rlyeh.sky().eldritch || !rlyeh.fog().enabled || rlyeh.emitters().size() < 10) {
        std::cerr << "R'lyeh map missing sky, fog, or emitters\n";
        return 1;
    }
    rlyeh.animate(0.0);
    const double spineAt0 = rlyeh.at(22, 33).floor;
    const double monolithAt0 = rlyeh.at(17, 12).wallHeight;
    const int morphWallAt0 = rlyeh.at(17, 12).wall;
    const int morphFloorAt0 = rlyeh.at(22, 35).surface;
    rlyeh.animate(3.0);
    const double spineAt3 = rlyeh.at(22, 33).floor;
    const double monolithAt3 = rlyeh.at(17, 12).wallHeight;
    rlyeh.animate(5.0);
    const int morphWallAt5 = rlyeh.at(17, 12).wall;
    const int morphFloorAt5 = rlyeh.at(22, 35).surface;
    if (std::abs(spineAt0 - spineAt3) < 0.1 || std::abs(monolithAt0 - monolithAt3) < 0.5 ||
        morphWallAt0 == morphWallAt5 || morphFloorAt0 == morphFloorAt5) {
        std::cerr << "R'lyeh dynamic terrain or texture morph failed to animate\n";
        return 1;
    }
    Camera cultist(rlyeh.spawnPosition(), rlyeh.spawnDirection());
    Screen eldritchView(120, 42);
    rlyeh.animate(1.5);
    cultist.render(rlyeh, eldritchView, nullptr, 1.5);
    const auto rlyehFrame1 = eldritchView.toDisplayString();
    if (argc > 1 && (std::string(argv[1]) == "--rlyeh" || std::string(argv[1]) == "--hud" ||
                     std::string(argv[1]) == "--title" || std::string(argv[1]) == "--hud-large" ||
                     std::string(argv[1]) == "--title-large")) {
        const std::string mode = argv[1];
        const bool large = (mode == "--hud-large" || mode == "--title-large");
        Screen preview(large ? 220 : 120, large ? 76 : 42);
        cultist.render(rlyeh, preview, nullptr, 1.5);
        if (mode != "--rlyeh") {
            HudUi hud({{"R'LYEH", "Cthulhu Horror", "assets/maps/rlyeh.map", "assets/textures/rlyeh.atlas"}}, 0);
            if (mode == "--hud" || mode == "--hud-large") {
                InputState enter{};
                enter.menuConfirm = true;
                hud.update(enter, 0.016);
            }
            hud.draw(preview, rlyeh, cultist.pose(), "rlyeh.map", 1.5, 428.0, true, false);
        }
        std::cout << preview.toDisplayString() << '\n';
        return 0;
    }
    unsigned rlyehColors = 0;
    int skyStars = 0;
    for (int y = 1; y <= eldritchView.height(); ++y) {
        for (int x = 1; x <= eldritchView.width(); ++x) {
            auto cell = eldritchView.framedCell(x, y);
            if (cell.color < 16 && cell.glyph != ' ') rlyehColors |= 1u << cell.color;
            if (y <= 14 && cell.glyph != ' ') ++skyStars;
        }
    }
    int colorCount = 0;
    for (int c = 0; c < 16; ++c) if (rlyehColors & (1u << c)) ++colorCount;
    if (colorCount < 6 || skyStars < 20) {
        std::cerr << "R'lyeh frame lacks rich colors or starry sky (colors=" << colorCount << ", sky=" << skyStars << ")\n";
        return 1;
    }
    eldritchView.clear();
    rlyeh.animate(4.2);
    cultist.render(rlyeh, eldritchView, nullptr, 4.2);
    if (rlyehFrame1 == eldritchView.toDisplayString()) {
        std::cerr << "R'lyeh scene did not animate over time\n";
        return 1;
    }

    WorldMap well("assets/maps/star_well.map", "assets/textures/star_well.atlas");
    if(well.swarms().size()!=2)return 1;
    well.animate(4);if(!(well.at(20,21).floor>well.at(20,16).floor)) return 1;
    Camera witness(well.spawnPosition(),well.spawnDirection());Screen vista(100,40);
    witness.render(well,vista,nullptr,1);auto first=vista.toDisplayString();vista.clear();
    witness.render(well,vista,nullptr,3);if(first==vista.toDisplayString())return 1;
    WorldMap court("assets/maps/lantern_court.map", "assets/textures/lantern_court.atlas");
    for(auto [time,height]: {std::pair<double,double>{0,.45},{6,1.95},{12,.45}}){
        court.animate(time);if(std::abs(court.at(18,16).floor-height)>1e-6 || court.at(35,16).floor!=.6)return 1;
    }
    WorldMap map("assets/maps/foundry.map", "assets/textures/industrial.atlas");
    Camera camera(map.spawnPosition(), map.spawnDirection());
    Screen screen(100, 40);
    auto render = [&] {
        screen.clear();
        camera.render(map, screen);
        return screen.toDisplayString();
    };
    const auto spawn = render();
    if (argc > 1 && std::string(argv[1]) == "--preview") std::cout << screen.toAnsiString() << '\n';
    if (spawn != render() || spawn.size() != 103u * 42u - 1u) {
        std::cerr << "Spawn frame unstable or malformed\n";
        return 1;
    }
    PlayerPose guest{4.5,5.5,0,0,0};
    auto guestFrame = [&] {screen.clear(); camera.render(map,screen,&guest); return screen.toDisplayString();};
    const auto standing = guestFrame();
    guest.feet = 0.6;
    if (standing == spawn || guestFrame() == standing) {std::cerr << "Remote avatar or jump not visible\n";return 1;}
    guest.feet = 0; // A jumping head may legitimately rise above a one-unit wall.
    guest.x = 17.5;
    if (guestFrame() != spawn) {std::cerr << "Avatar visible through boundary wall\n";return 1;}
    render();
    unsigned colors = 0;
    for (int y = 1; y <= screen.height(); ++y) {
        for (int x = 1; x <= screen.width(); ++x) {
            auto cell = screen.framedCell(x, y);
            if (cell.color < 16 && cell.glyph != ' ') colors |= 1u << cell.color;
        }
    }
    if (!(colors & (colors - 1))) {
        std::cerr << "Spawn has no distinguishable material colors\n";
        return 1;
    }
    for (int i = 0; i < 20; ++i) camera.advance(map,1.0/60,0,i==0);
    if (render() == spawn) { std::cerr << "Jump never left ground\n"; return 1; }
    for (int i = 0; i < 60; ++i) camera.advance(map,1.0/60,0,false);
    if (render() != spawn) { std::cerr << "Jump failed to land\n"; return 1; }
    camera.look(0,-1000);
    const auto up = render();
    camera.look(0,-1000);
    if (up==spawn || render()!=up) { std::cerr << "Pitch limit failed\n"; return 1; }
    camera.look(0,1000);
    if (render()==up) { std::cerr << "Cannot look down\n"; return 1; }
    camera.turn(0.35);
    if (spawn == render()) {
        std::cerr << "Scene did not change after turning\n";
        return 1;
    }
    WorldMap sky("assets/maps/skyway.map", "assets/textures/industrial.atlas");
    Camera runner(sky.spawnPosition(),sky.spawnDirection());
    auto view = [&]{ screen.clear(); runner.render(sky,screen); return screen.toDisplayString(); };
    const auto startView = view();
    // Walk off the east edge and let gravity return the player to spawn.
    for(int i=0;i<50;++i) runner.advance(sky,1.0/60,1,false);
    for(int i=0;i<90;++i) runner.advance(sky,1.0/60,0,false);
    if (view()!=startView) { std::cerr << "Pit did not reset player\n"; return 1; }
    // Jump from the starting ledge to the first raised island.
    for(int i=0;i<40;++i) runner.advance(sky,1.0/60,1,false);
    for(int i=0;i<50;++i) runner.advance(sky,1.0/60,1,i==0);
    const auto landed = view();
    for(int i=0;i<80;++i) runner.advance(sky,1.0/60,0,false);
    if (view()==startView || view()!=landed) { std::cerr << "First jump failed\n"; return 1; }
    return 0;
}
