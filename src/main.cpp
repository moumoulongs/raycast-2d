#include "app.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;
namespace {
std::string trim(const std::string& s) {
    auto first = s.find_first_not_of(" \r\t");
    return first == s.npos ? "" : s.substr(first, s.find_last_not_of(" \r\t") - first + 1);
}
// Hash normalized resource lines so LF and CRLF copies can play together.
std::uint64_t resourceFingerprint(const fs::path& map, const fs::path& atlas) {
    std::uint64_t hash = 14695981039346656037ULL;
    auto add = [&](unsigned char c) { hash = (hash ^ c) * 1099511628211ULL; };
    for (const auto& path : {map, atlas}) {
        std::ifstream in(path, std::ios::binary);
        if (!in) throw std::runtime_error("Cannot fingerprint: " + path.string());
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            for (unsigned char c : line) add(c);
            add('\n');
        }
        add(0);
    }
    return hash;
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
                if (cfg.font == 0 || (cfg.font >= 3 && cfg.font <= 32)) continue;
            }
        }
        throw std::runtime_error(path.string() + ":" + std::to_string(number) + ": invalid setting: " + line);
    }
    if (cfg.map.empty() || cfg.textures.empty()) throw std::runtime_error(path.string() + ": missing map or textures");
    return cfg;
}
std::vector<RealmEntry> discoverRealms(const fs::path& assetDir, const Settings& activeCfg, int& activeIndex) {
    struct Known { const char* stem; const char* label; const char* sub; const char* atlas; };
    static const Known kKnown[] = {
        {"rlyeh", "R'LYEH", "Cthulhu Horror / Starry Cosmos", "rlyeh.atlas"},
        {"star_well", "STAR WELL", "Sunken Shrine & Spiral Swarms", "star_well.atlas"},
        {"lantern_court", "LANTERN COURT", "Shifting Courtyard & Lifts", "lantern_court.atlas"},
        {"skyway", "SKYWAY", "Suspended Parkour Islands", "industrial.atlas"},
        {"foundry", "FOUNDRY", "Industrial Complex", "industrial.atlas"},
        {"tideworks", "TIDEWORKS", "Tidal Sluiceway", "industrial.atlas"},
    };
    std::vector<RealmEntry> out;
    activeIndex = 0;
    const fs::path mapsDir = assetDir / "maps";
    const fs::path texDir = assetDir / "textures";
    const std::string activeStem = activeCfg.map.stem().u8string();
    for (const auto& k : kKnown) {
        const fs::path mp = mapsDir / (std::string(k.stem) + ".map");
        const fs::path tp = texDir / k.atlas;
        if (fs::exists(mp) && fs::exists(tp)) {
            if (activeStem == k.stem) activeIndex = static_cast<int>(out.size());
            out.push_back({k.label, k.sub, mp, tp});
        }
    }
    if (out.empty()) {
        out.push_back({activeCfg.map.stem().u8string(), "Custom Map", activeCfg.map, activeCfg.textures});
    }
    return out;
}
}
int main(int argc, char** argv) {
    try {
        fs::path config = fs::absolute(argv[0]).parent_path() / "assets/game.cfg";
        if (argc == 3 && std::string(argv[1]) == "--config") config = argv[2];
        else if (argc != 1) throw std::runtime_error("Usage: raycast-2d [--config FILE]");
        auto cfg = load(config);
        int activeRealm = 0;
        auto realms = discoverRealms(config.parent_path(), cfg, activeRealm);
        WorldMap map(cfg.map, cfg.textures);
        App app(createPlatform(cfg.font), std::move(map), resourceFingerprint(cfg.map,cfg.textures),
                cfg.map.filename().u8string(), std::move(realms), activeRealm);
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "raycast-2d: " << e.what() << '\n';
#ifdef _WIN32
        MessageBoxA(nullptr, e.what(), "raycast-2d", MB_OK | MB_ICONERROR);
#endif
        return 1;
    }
}
