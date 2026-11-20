#include "app.h"
#include "game_resource.h"
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
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
bool nextLine(std::string_view& src, std::string& out) {
    if (src.empty()) return false;
    const auto nl = src.find('\n');
    std::string_view sv = (nl == std::string_view::npos) ? src : src.substr(0, nl);
    src = (nl == std::string_view::npos) ? std::string_view{} : src.substr(nl + 1);
    if (!sv.empty() && sv.back() == '\r') sv.remove_suffix(1);
    out.assign(sv.data(), sv.size());
    return true;
}
// Hash normalized resource lines so LF and CRLF copies can play together.
std::uint64_t resourceFingerprint(const fs::path& map, const fs::path& atlas) {
    std::uint64_t hash = 14695981039346656037ULL;
    auto add = [&](unsigned char c) { hash = (hash ^ c) * 1099511628211ULL; };
    for (const auto& path : {map, atlas}) {
        std::string data;
        if (!readGameResource(path, data)) throw std::runtime_error("Cannot fingerprint: " + path.string());
        std::string_view view(data);
        std::string line;
        while (nextLine(view, line)) {
            for (unsigned char c : line) add(c);
            add('\n');
        }
        add(0);
    }
    return hash;
}
struct Settings { fs::path map, textures; int font = 8; int fpsCap = 500; };
Settings load(const fs::path& path) {
    std::string data;
    if (!readGameResource(path, data)) throw std::runtime_error("Cannot open config: " + path.string());
    Settings cfg;
    std::string_view view(data);
    std::string line;
    int number = 0;
    while (nextLine(view, line)) {
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
            if (key == "fps_cap" && value.size() <= 4 && value.find_first_not_of("0123456789") == value.npos) {
                cfg.fpsCap = std::stoi(value);
                if (cfg.fpsCap >= 0 && cfg.fpsCap <= 2000) continue;
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
        {"drowned_light", "DROWNED LIGHT", "Seamless Story World (ZH/EN)", "drowned_light.atlas"},
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
        if (gameResourceExists(mp) && gameResourceExists(tp)) {
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
        fs::path config = fs::path(":builtin:") / "game.cfg";
        if (argc == 3 && std::string(argv[1]) == "--config") config = argv[2];
        else if (argc != 1 && !(argc == 3 && std::string(argv[1]) == "--map")) throw std::runtime_error("Usage: raycast-2d [--config FILE | --map FILE]");
        auto cfg = load(config);
        int activeRealm = 0;
        auto realms = discoverRealms(config.parent_path(), cfg, activeRealm);
        if (argc == 3 && std::string(argv[1]) == "--map") {
            cfg.map = fs::absolute(argv[2]);
            auto atlas = cfg.map;
            atlas.replace_extension(".atlas");
            if (fs::exists(atlas)) cfg.textures = atlas;
            // Without a sidecar, keep the atlas selected by the built-in config.
            activeRealm = static_cast<int>(realms.size());
            realms.push_back({cfg.map.stem().u8string(), "Custom Map", cfg.map, cfg.textures});
        }
        WorldMap map = [&] {
            try { return WorldMap(cfg.map, cfg.textures); }
            catch (const std::exception& e) {
                if (argc != 3 || std::string(argv[1]) != "--map") throw;
                auto sidecar = cfg.map;
                sidecar.replace_extension(".atlas");
                if (cfg.textures == sidecar) throw;
                throw std::runtime_error(std::string(e.what()) + "\nUsing the built-in atlas. For custom textures, supply: " + sidecar.string());
            }
        }();
        App app(createPlatform(cfg.font), std::move(map), resourceFingerprint(cfg.map,cfg.textures),
                cfg.map.filename().u8string(), std::move(realms), activeRealm, cfg.fpsCap, config.parent_path());
        return app.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "raycast-2d: %s\n", e.what());
#ifdef _WIN32
        MessageBoxA(nullptr, e.what(), "raycast-2d", MB_OK | MB_ICONERROR);
#endif
        return 1;
    }
}
