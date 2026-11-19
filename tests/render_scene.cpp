#include "camera.h"
#include <iostream>

int main(int argc, char** argv) {
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
