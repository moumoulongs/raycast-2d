#include "world_map.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "game_resource.h"
#include <stdexcept>
#include <string_view>

namespace {
bool nextLine(std::string_view& src, std::string& out) {
    if (src.empty()) return false;
    const auto nl = src.find('\n');
    std::string_view sv = (nl == std::string_view::npos) ? src : src.substr(0, nl);
    src = (nl == std::string_view::npos) ? std::string_view{} : src.substr(nl + 1);
    if (!sv.empty() && sv.back() == '\r') sv.remove_suffix(1);
    out.assign(sv.data(), sv.size());
    return true;
}

class LineScanner {
public:
    explicit LineScanner(const std::string& line) : p_(line.c_str()) {}
    explicit operator bool() const { return ok_; }
    LineScanner& operator>>(std::string& out) {
        skipWs();
        if (!ok_ || !*p_) { ok_ = false; return *this; }
        const char* s = p_;
        while (*p_ && !isWs(*p_)) ++p_;
        out.assign(s, static_cast<std::size_t>(p_ - s));
        return *this;
    }
    LineScanner& operator>>(char& out) {
        skipWs();
        if (!ok_ || !*p_) { ok_ = false; return *this; }
        out = *p_++;
        return *this;
    }
    LineScanner& operator>>(int& out) {
        skipWs();
        if (!ok_ || !*p_) { ok_ = false; return *this; }
        char* end = nullptr;
        const long v = std::strtol(p_, &end, 10);
        if (end == p_ || (*end != '\0' && !isWs(*end))) { ok_ = false; return *this; }
        p_ = end;
        out = static_cast<int>(v);
        return *this;
    }
    LineScanner& operator>>(double& out) {
        skipWs();
        if (!ok_ || !*p_) { ok_ = false; return *this; }
        char* end = nullptr;
        const double v = std::strtod(p_, &end);
        if (end == p_ || (*end != '\0' && !isWs(*end))) { ok_ = false; return *this; }
        p_ = end;
        out = v;
        return *this;
    }
    LineScanner& operator>>(float& out) {
        double v = 0.0;
        *this >> v;
        if (ok_) out = static_cast<float>(v);
        return *this;
    }
private:
    static bool isWs(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
    void skipWs() { while (*p_ && isWs(*p_)) ++p_; }
    const char* p_ = "";
    bool ok_ = true;
};
constexpr double kTwoPi = 6.28318530717958647692;

inline bool hasStoryFlag(std::uint64_t flags, std::uint8_t flagNum) {
    return flagNum == 0 || ((flags & (std::uint64_t{1} << (flagNum - 1))) != 0);
}

double wrapPhase(double v) {
    const double r = std::fmod(v, 1.0);
    return r < 0 ? r + 1.0 : r;
}

int parseHexColor(char c, unsigned char fallback) {
    if (c == '.' || c == ' ') return fallback;
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool parseEmitterKind(const std::string& name, Emitter::Kind& out) {
    if (name == "vortex") { out = Emitter::Kind::Vortex; return true; }
    if (name == "rain") { out = Emitter::Kind::Rain; return true; }
    if (name == "spores") { out = Emitter::Kind::Spores; return true; }
    if (name == "geyser") { out = Emitter::Kind::Geyser; return true; }
    if (name == "watchers") { out = Emitter::Kind::Watchers; return true; }
    if (name == "lightning") { out = Emitter::Kind::Lightning; return true; }
    if (name == "halo") { out = Emitter::Kind::Halo; return true; }
    return false;
}
} // namespace

void WorldMap::animate(double seconds) {
    for (const auto& lift : lifts_) {
        const double phase = wrapPhase(seconds / lift.period + lift.offset);
        // Hold at both landings, easing in and out between them.
        const double t = phase < .25 ? 0 : phase < .5 ? (phase - .25) * 4 : phase < .75 ? 1 : (1 - phase) * 4;
        tiles_[lift.tileIdx].floor = static_cast<double>(lift.low) + (static_cast<double>(lift.high) - lift.low) * t * t * (3 - 2 * t);
    }
    for (const auto& wave : waves_) {
        const double phase = wrapPhase(seconds / wave.period + wave.offset);
        const double t = 0.5 - 0.5 * std::cos(phase * kTwoPi);
        tiles_[wave.tileIdx].floor = static_cast<double>(wave.low) + (static_cast<double>(wave.high) - wave.low) * t;
    }
    for (const auto& m : monoliths_) {
        const double phase = wrapPhase(seconds / m.period + m.offset);
        const double t = 0.5 - 0.5 * std::cos(phase * kTwoPi);
        tiles_[m.tileIdx].wallHeight = static_cast<float>(m.lowH + (m.highH - m.lowH) * t);
    }
    for (const auto& morph : morphs_) {
        const double phase = wrapPhase(seconds / morph.period + morph.offset);
        const bool alt = phase >= 0.5;
        auto& tile = tiles_[morph.tileIdx];
        tile.wall = alt ? morph.wall2 : morph.wall1;
        tile.surface = alt ? morph.surf2 : morph.surf1;
        tile.side = alt ? morph.side2 : morph.side1;
    }
}

void WorldMap::updateStoryWorld(double dt, std::uint64_t flags, double playerX, double playerY, int skyOverride) {
    // 1. Smoothly animate story-driven gates & drawbridges.
    for (auto& g : gates_) {
        const float target = hasStoryFlag(flags, g.reqFlag) ? 1.0f : 0.0f;
        const float step = static_cast<float>(std::clamp(dt, 0.0, 0.1)) * g.speed;
        if (g.currentT < target) g.currentT = std::min(target, g.currentT + step);
        else if (g.currentT > target) g.currentT = std::max(target, g.currentT - step);
        const double smooth = static_cast<double>(g.currentT) * g.currentT * (3.0 - 2.0 * g.currentT);
        tiles_[g.tileIdx].floor = static_cast<double>(g.closedFloor) +
            (static_cast<double>(g.openFloor) - g.closedFloor) * smooth;
    }

    // 2. Smoothly blend regional atmosphere (sky & fog) for seamless open-world zones.
    if (!zones_.empty()) {
        const MapZone* active = nullptr;
        for (const auto& z : zones_) {
            if (!hasStoryFlag(flags, z.reqFlag)) continue;
            if (playerX >= z.x0 && playerX <= z.x1 && playerY >= z.y0 && playerY <= z.y1) {
                active = &z;
            }
        }
        const float blend = static_cast<float>(std::clamp(dt * 1.8, 0.0, 1.0));
        if (active) {
            fog_.enabled = true;
            fog_.start += (active->fogStart - fog_.start) * blend;
            fog_.end += (active->fogEnd - fog_.end) * blend;
            fog_.color = active->fogColor;
            sky_.enabled = (active->skyMode != 0);
            sky_.eldritch = (active->skyMode == 2);
        } else {
            fog_ = baseFog_;
            sky_ = baseSky_;
        }
    }
    if (skyOverride == 1) { sky_.enabled = true; sky_.eldritch = false; }
    else if (skyOverride == 2) { sky_.enabled = true; sky_.eldritch = true; }

    // 3. Select active conditional compass beacon.
    if (!beacons_.empty()) {
        for (const auto& b : beacons_) {
            if (hasStoryFlag(flags, b.reqFlag) && (b.hideFlag == 0 || !hasStoryFlag(flags, b.hideFlag))) {
                beacon_ = vec2(b.x, b.y);
                hasBeacon_ = true;
                break;
            }
        }
    }
}

WorldMap::WorldMap(const std::filesystem::path& mapFile,const std::filesystem::path& textureFile) {
    std::string mapData, atlasData;
    if (!readGameResource(textureFile, atlasData)) throw std::runtime_error("Invalid or missing resource: " + textureFile.string());
    if (!readGameResource(mapFile, mapData)) throw std::runtime_error("Invalid or missing resource: " + mapFile.string());
    *this = WorldMap(mapData, atlasData, mapFile, textureFile);
}
WorldMap::WorldMap(std::string_view file,std::string_view atlas,const std::filesystem::path& mapFile,const std::filesystem::path& textureFile) {
    std::string line;
    auto bad=[&](const auto& path){ throw std::runtime_error("Invalid or missing resource: "+path.string()+" near: "+line); };
    // Slot 0 is reserved as empty/none sentinel so real textures occupy slots 1..255.
    textures_.push_back(WorldTexture{});
    auto slotOf = [&](int id) -> std::uint8_t {
        return (id > 0 && id < static_cast<int>(texSlots_.size())) ? texSlots_[id] : std::uint8_t{0};
    };
    while(nextLine(atlas,line)) {
        if(line.empty() || line[0]==';') continue;
        LineScanner header(line); int id,color,w,h;
        if(!(header>>id>>color>>w>>h)||id<=0||id>4096||slotOf(id)!=0||textures_.size()>=255||color<0||color>15||w<1||h<1||w>256||h>256) bad(textureFile);
        WorldTexture tex{};
        tex.texelOffset = static_cast<std::uint32_t>(texelAtlas_.size());
        tex.width = static_cast<std::uint16_t>(w);
        tex.height = static_cast<std::uint16_t>(h);
        tex.frameStride = static_cast<std::uint32_t>(w) * static_cast<std::uint32_t>(h);
        tex.color = static_cast<std::uint8_t>(color);
        bool hasColors = false;
        std::string effect;
        while (header >> effect) {
            if (effect == "flow") {
                if (!(header >> tex.flowX >> tex.flowY) || !std::isfinite(tex.flowX) || !std::isfinite(tex.flowY) || std::abs(tex.flowX)>8 || std::abs(tex.flowY)>8) bad(textureFile);
                if (tex.flowX != 0.0f || tex.flowY != 0.0f) tex.flags |= 1u;
            } else if (effect == "anim") {
                int count = 0;
                if (!(header >> tex.fps >> count) || !std::isfinite(tex.fps) || tex.fps <= 0 || tex.fps > 30 || count < 1 || count > 32) bad(textureFile);
                tex.frames = static_cast<std::uint16_t>(count);
                if (count > 1) tex.flags |= 2u;
            } else if (effect == "colors") {
                hasColors = true;
            } else {
                bad(textureFile);
            }
        }
        const size_t totalCells = static_cast<size_t>(tex.frames) * w * h;
        texelAtlas_.resize(tex.texelOffset + totalCells);
        for (int f = 0; f < tex.frames; ++f) {
            const size_t base = tex.texelOffset + static_cast<size_t>(f) * tex.frameStride;
            for (int r = 0; r < h; ++r) {
                if(!nextLine(atlas,line)) bad(textureFile);
                if(int(line.size())!=w) bad(textureFile);
                for (int c = 0; c < w; ++c) {
                    texelAtlas_[base + static_cast<size_t>(r) * w + c] = {line[c], tex.color};
                }
            }
            if (hasColors) {
                for (int r = 0; r < h; ++r) {
                    if(!nextLine(atlas,line)) bad(textureFile);
                    if(int(line.size())!=w) bad(textureFile);
                    for (int c = 0; c < w; ++c) {
                        const int parsed = parseHexColor(line[c], tex.color);
                        if (parsed < 0) bad(textureFile);
                        texelAtlas_[base + static_cast<size_t>(r) * w + c].color = static_cast<unsigned char>(parsed);
                    }
                }
            }
        }
        if (id >= static_cast<int>(texSlots_.size())) texSlots_.resize(static_cast<size_t>(id) + 1, 0);
        texSlots_[id] = static_cast<std::uint8_t>(textures_.size());
        textures_.push_back(tex);
    }

    std::array<std::int16_t, 256> symToTile;
    symToTile.fill(-1);
    auto tileIdxFor = [&](char ch) -> int { return symToTile[static_cast<unsigned char>(ch)]; };

    std::vector<std::string> rawRows;
    bool spawn=false;
    while(nextLine(file,line)) {
        LineScanner row(line); std::string op; if(!(row>>op)||op[0]==';')continue;
        if(op=="tile"){
            char c; double fl = 0, wh = 1.0; int wall = 0, surf = 0, sideTex = 0;
            if(!(row>>c>>fl>>wall>>surf)||std::abs(fl)>32||!slotOf(surf)||(wall&&!slotOf(wall))) bad(mapFile);
            if (row >> wh) {
                if (!std::isfinite(wh) || wh <= 0 || wh > 16) bad(mapFile);
                if (row >> sideTex) {
                    if (sideTex < 0 || (sideTex && !slotOf(sideTex))) bad(mapFile);
                }
            }
            if (tileIdxFor(c) >= 0 || tiles_.size() >= 255) bad(mapFile);
            symToTile[static_cast<unsigned char>(c)] = static_cast<std::int16_t>(tiles_.size());
            Tile t{};
            t.floor = fl;
            t.wallHeight = static_cast<float>(wh);
            t.wall = wall ? slotOf(wall) : std::uint8_t{0};
            t.surface = slotOf(surf);
            t.side = sideTex ? slotOf(sideTex) : std::uint8_t{0};
            tiles_.push_back(t);
        }
        else if(op=="lift"){
            char sym; double lo, hi, per, off;
            if(!(row>>sym>>lo>>hi>>per>>off)) bad(mapFile);
            const int idx = tileIdxFor(sym);
            if(idx<0 || tiles_[idx].wall || !(lo>=-32&&hi<=32&&hi>lo&&per>=1&&per<=300&&off>=0&&off<1)) bad(mapFile);
            lifts_.push_back({static_cast<float>(lo), static_cast<float>(hi), static_cast<float>(per), static_cast<float>(off), static_cast<std::uint8_t>(idx)});
        }
        else if(op=="wave"){
            char sym; double lo, hi, per, off;
            if(!(row>>sym>>lo>>hi>>per>>off)) bad(mapFile);
            const int idx = tileIdxFor(sym);
            if(idx<0 || tiles_[idx].wall || !(lo>=-32&&hi<=32&&hi>lo&&per>=1&&per<=300&&off>=0&&off<1)) bad(mapFile);
            waves_.push_back({static_cast<float>(lo), static_cast<float>(hi), static_cast<float>(per), static_cast<float>(off), static_cast<std::uint8_t>(idx)});
        }
        else if(op=="monolith"){
            char sym; double lo, hi, per, off;
            if(!(row>>sym>>lo>>hi>>per>>off)) bad(mapFile);
            const int idx = tileIdxFor(sym);
            if(idx<0 || !tiles_[idx].wall || !(lo>0&&hi<=16&&hi>lo&&per>=1&&per<=300&&off>=0&&off<1)) bad(mapFile);
            monoliths_.push_back({static_cast<float>(lo), static_cast<float>(hi), static_cast<float>(per), static_cast<float>(off), static_cast<std::uint8_t>(idx)});
        }
        else if(op=="morph"){
            char sym; double per, off; int w2 = 0, s2 = 0, sd2 = -1;
            if(!(row>>sym>>per>>off>>w2>>s2)) bad(mapFile);
            const int idx = tileIdxFor(sym);
            if(idx<0 || !(per>=1&&per<=300&&off>=0&&off<1)||!slotOf(s2)||(w2&&!slotOf(w2))) bad(mapFile);
            const auto& base = tiles_[idx];
            if ((base.wall != 0) != (w2 != 0)) bad(mapFile);
            Morph m{};
            m.period = static_cast<float>(per);
            m.offset = static_cast<float>(off);
            m.tileIdx = static_cast<std::uint8_t>(idx);
            m.wall1 = base.wall; m.surf1 = base.surface; m.side1 = base.side;
            m.wall2 = w2 ? slotOf(w2) : std::uint8_t{0};
            m.surf2 = slotOf(s2);
            m.side2 = base.side;
            if (row >> sd2) { if (sd2 < 0 || (sd2 && !slotOf(sd2))) bad(mapFile); m.side2 = sd2 ? slotOf(sd2) : std::uint8_t{0}; }
            morphs_.push_back(m);
        }
        else if(op=="swarm") {
            double x, y, r, h; int cnt = 0;
            if(!(row>>x>>y>>r>>h>>cnt) || !(x>=0&&x<512&&y>=0&&y<512&&r>0&&r<=16&&h>0&&h<=16) || cnt<1 || cnt>128 || swarms_.size()>=16) bad(mapFile);
            swarms_.push_back({static_cast<float>(x), static_cast<float>(y), static_cast<float>(r), static_cast<float>(h), static_cast<std::uint16_t>(cnt), 0});
        }
        else if(op=="emitter") {
            std::string kindName; double x, y, r, z0, z1, spd = 1.0; int cnt = 0, c1 = 0, c2 = 0;
            Emitter::Kind kind{};
            if(!(row>>kindName>>x>>y>>r>>z0>>z1>>cnt>>c1>>c2) || !parseEmitterKind(kindName, kind) ||
               !(x>=0&&x<512&&y>=0&&y<512&&r>0&&r<=32&&z0>=-16&&z1<=24&&z1>=z0) ||
               cnt<1 || cnt>256 || c1<0 || c1>15 || c2<0 || c2>15 || emitters_.size()>=128) bad(mapFile);
            if (row >> spd) { if (!std::isfinite(spd) || spd <= 0 || spd > 16) bad(mapFile); }
            Emitter e{};
            e.x = static_cast<float>(x); e.y = static_cast<float>(y); e.radius = static_cast<float>(r);
            e.zMin = static_cast<float>(z0); e.zMax = static_cast<float>(z1); e.speed = static_cast<float>(spd);
            e.count = static_cast<std::uint16_t>(cnt); e.kind = kind;
            e.color1 = static_cast<std::uint8_t>(c1); e.color2 = static_cast<std::uint8_t>(c2);
            emitters_.push_back(e);
        }
        else if(op=="sky") {
            std::string mode;
            if(!(row>>mode) || (mode != "eldritch" && mode != "starry")) bad(mapFile);
            sky_.enabled = true;
            sky_.eldritch = (mode == "eldritch");
            baseSky_ = sky_;
        }
        else if(op=="fog") {
            double st = 0, en = 0; int c = 8;
            if(!(row>>st>>en>>c) || !(st>=0 && en>st && en<=128) || c<0 || c>15) bad(mapFile);
            fog_.enabled = true;
            fog_.start = static_cast<float>(st);
            fog_.end = static_cast<float>(en);
            fog_.color = static_cast<std::uint8_t>(c);
            baseFog_ = fog_;
        }
        else if(op=="zone") {
            double x0, y0, x1, y1, fs, fe; std::string skyMode; int fc = 6, req = 0;
            if(!(row>>x0>>y0>>x1>>y1>>skyMode>>fs>>fe>>fc) ||
               !(x0<x1 && y0<y1 && fs>=0 && fe>fs && fe<=128 && fc>=0 && fc<=15) ||
               (skyMode!="none" && skyMode!="starry" && skyMode!="eldritch") || zones_.size()>=32) bad(mapFile);
            if (row >> req) { if (req < 0 || req > 63) bad(mapFile); }
            MapZone z{};
            z.x0 = static_cast<float>(x0); z.y0 = static_cast<float>(y0);
            z.x1 = static_cast<float>(x1); z.y1 = static_cast<float>(y1);
            z.fogStart = static_cast<float>(fs); z.fogEnd = static_cast<float>(fe);
            z.skyMode = (skyMode == "eldritch") ? 2u : (skyMode == "starry" ? 1u : 0u);
            z.fogColor = static_cast<std::uint8_t>(fc);
            z.reqFlag = static_cast<std::uint8_t>(req);
            zones_.push_back(z);
        }
        else if(op=="gate") {
            char sym; int req = 0; double cFl = 0, oFl = 0, spd = 1.0;
            if(!(row>>sym>>req>>cFl>>oFl)) bad(mapFile);
            if (row >> spd) { if (!std::isfinite(spd) || spd <= 0 || spd > 10) bad(mapFile); }
            const int idx = tileIdxFor(sym);
            if(idx<0 || tiles_[idx].wall || req<1 || req>63 || std::abs(cFl)>32 || std::abs(oFl)>32) bad(mapFile);
            tiles_[idx].floor = cFl;
            gates_.push_back({static_cast<float>(cFl), static_cast<float>(oFl), 0.0f, static_cast<float>(spd),
                              static_cast<std::uint8_t>(idx), static_cast<std::uint8_t>(req), {}});
        }
        else if(op=="trigger") {
            std::string kindName, eventName; double x, y, r; int req = 0, forbid = 0;
            if(!(row>>kindName>>x>>y>>r>>req>>forbid>>eventName) ||
               (kindName!="interact" && kindName!="enter") ||
               !(x>=0&&x<512&&y>=0&&y<512&&r>0&&r<=32&&req>=0&&req<=63&&forbid>=0&&forbid<=63) ||
               triggers_.size()>=128) bad(mapFile);
            const auto evIdx = static_cast<std::uint16_t>(triggerEvents_.size());
            triggerEvents_.push_back(std::move(eventName));
            MapTrigger tr{};
            tr.x = static_cast<float>(x); tr.y = static_cast<float>(y); tr.radius = static_cast<float>(r);
            tr.eventIndex = evIdx;
            tr.kind = (kindName == "enter") ? 1u : 0u;
            tr.reqFlag = static_cast<std::uint8_t>(req);
            tr.forbidFlag = static_cast<std::uint8_t>(forbid);
            triggers_.push_back(tr);
        }
        else if(op=="story") {
            if(!(row>>storyFile_) || storyFile_.empty()) bad(mapFile);
        }
        else if(op=="spawn"){if(!(row>>start_.x>>start_.y>>direction_.x>>direction_.y)||direction_.magnitudeSquared()<0.0001)bad(mapFile); spawn=true;}
        else if(op=="beacon"){
            double bx, by; int req = 0, hide = 0;
            if(!(row>>bx>>by)||bx<0||by<0||bx>=512||by>=512)bad(mapFile);
            if (row >> req) {
                if (req < 0 || req > 63) bad(mapFile);
                if (row >> hide) { if (hide < 0 || hide > 63) bad(mapFile); }
            }
            if (!hasBeacon_) { beacon_ = vec2(bx, by); hasBeacon_ = true; }
            beacons_.push_back({static_cast<float>(bx), static_cast<float>(by),
                                static_cast<std::uint8_t>(req), static_cast<std::uint8_t>(hide), 9, {}});
        }
        else if(op=="prop"){
            int tid = 0; double x, y, w, h, amp = 0, spd = 0, off = 0;
            if(!(row>>tid>>x>>y>>w>>h)||!slotOf(tid)||w<=0||h<=0) bad(mapFile);
            std::string mod;
            if (row >> mod) {
                if (mod != "float" || !(row >> amp >> spd) || !std::isfinite(amp) || !std::isfinite(spd) || std::abs(amp) > 8 || std::abs(spd) > 20) bad(mapFile);
                if (row >> off) {
                    if (!std::isfinite(off) || std::abs(off) > 16) bad(mapFile);
                }
            }
            WorldProp p{};
            p.x = static_cast<float>(x); p.y = static_cast<float>(y);
            p.width = static_cast<float>(w); p.height = static_cast<float>(h);
            p.floatAmp = static_cast<float>(amp); p.floatSpeed = static_cast<float>(spd);
            p.baseOffset = static_cast<float>(off);
            p.textureId = static_cast<std::uint16_t>(tid);
            p.textureSlot = slotOf(tid);
            props_.push_back(p);
        }
        else if(op=="row"){
            std::string cells;
            if(!(row>>cells)||cells.size()>512||rawRows.size()>=512||(!rawRows.empty()&&cells.size()!=rawRows[0].size()))bad(mapFile);
            for(char c:cells) if(tileIdxFor(c)<0) bad(mapFile);
            rawRows.push_back(std::move(cells));
        }
        else bad(mapFile);
    }
    if (!rawRows.empty()) {
        gridHeight_ = static_cast<int>(rawRows.size());
        gridWidth_ = static_cast<int>(rawRows[0].size());
        flatGrid_.resize(static_cast<size_t>(gridWidth_) * gridHeight_);
        for (int y = 0; y < gridHeight_; ++y) {
            for (int x = 0; x < gridWidth_; ++x) {
                flatGrid_[static_cast<size_t>(y) * gridWidth_ + x] = static_cast<std::uint8_t>(tileIdxFor(rawRows[y][x]));
            }
        }
    }
    auto walkable=[&](double x,double y){return gridWidth_>0&&gridHeight_>0&&x>=0&&y>=0&&x<gridWidth_&&y<gridHeight_&&!isWall(int(x),int(y));};
    if(!spawn||!walkable(start_.x,start_.y))bad(mapFile);
    for(const auto& p:props_)if(!walkable(p.x,p.y))bad(mapFile);
}
