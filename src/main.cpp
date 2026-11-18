#include "app.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace {
std::string trim(const std::string& s) {
    auto first = s.find_first_not_of(" \r\t");
    return first == s.npos ? "" : s.substr(first, s.find_last_not_of(" \r\t") - first + 1);
}
struct Settings { fs::path map, textures; int font = 8; };
Settings load(const fs::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open config: " + path.string());
    Settings cfg;
    std::string line;
    int number = 0;
    while (std::getline(in, line)) {
        ++number;
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        const auto eq = line.find('=');
        const auto key = trim(line.substr(0, eq));
        const auto value = eq == line.npos ? "" : trim(line.substr(eq + 1));
        if (!value.empty()) {
            if (key == "map") { cfg.map = path.parent_path() / fs::u8path(value); continue; }
            if (key == "textures") { cfg.textures = path.parent_path() / fs::u8path(value); continue; }
            if (key == "font_height" && value.size() <= 2 && value.find_first_not_of("0123456789") == value.npos) {
                cfg.font = std::stoi(value);
                if (cfg.font == 0 || (cfg.font >= 4 && cfg.font <= 32)) continue;
            }
        }
        throw std::runtime_error(path.string() + ":" + std::to_string(number) + ": invalid setting: " + line);
    }
    if (cfg.map.empty() || cfg.textures.empty()) throw std::runtime_error(path.string() + ": missing map or textures");
    return cfg;
}
}
int main(int argc, char** argv) {
    try {
        fs::path config = fs::absolute(argv[0]).parent_path() / "assets/game.cfg";
        if (argc == 3 && std::string(argv[1]) == "--config") config = argv[2];
        else if (argc != 1) throw std::runtime_error("Usage: raycast-2d [--config FILE]");
        auto cfg = load(config);
        WorldMap map(cfg.map, cfg.textures);
        App app(createPlatform(cfg.font), std::move(map));
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "raycast-2d: " << e.what() << '\n';
        return 1;
    }
}
