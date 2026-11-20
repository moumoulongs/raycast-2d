#include "story_runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "game_resource.h"
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

bool scanToken(const char*& p, std::string& out) {
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p) return false;
    const char* s = p;
    while (*p && *p != ' ' && *p != '\t') ++p;
    out.assign(s, static_cast<std::size_t>(p - s));
    return true;
}

bool scanInt(const char*& p, int& out) {
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p) return false;
    char* end = nullptr;
    const long v = std::strtol(p, &end, 10);
    if (end == p) return false;
    p = end;
    out = static_cast<int>(v);
    return true;
}

bool scanFloat(const char*& p, float& out) {
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p) return false;
    char* end = nullptr;
    const double v = std::strtod(p, &end);
    if (end == p) return false;
    p = end;
    out = static_cast<float>(v);
    return true;
}
std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

// Advance UTF-8 byte count without splitting multi-byte Chinese characters.
std::size_t utf8PrefixBytes(const std::string& s, std::size_t maxChars) {
    std::size_t i = 0, chars = 0;
    while (i < s.size() && chars < maxChars) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t step = 1;
        if ((c & 0xE0u) == 0xC0u) step = 2;
        else if ((c & 0xF0u) == 0xE0u) step = 3;
        else if ((c & 0xF8u) == 0xF0u) step = 4;
        if (i + step > s.size()) break;
        i += step;
        ++chars;
    }
    return i;
}

std::size_t utf8CharCount(const std::string& s) {
    std::size_t i = 0, chars = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t step = 1;
        if ((c & 0xE0u) == 0xC0u) step = 2;
        else if ((c & 0xF0u) == 0xE0u) step = 3;
        else if ((c & 0xF8u) == 0xF0u) step = 4;
        i += step;
        ++chars;
    }
    return chars;
}
} // namespace

bool StoryRuntime::load(const std::filesystem::path& storyPath) {
    nodes_.clear();
    reset();
    std::string content;
    if (!readGameResource(storyPath, content)) return false;
    std::string_view view(content);
    std::string line;
    StoryNode* cur = nullptr;
    while (nextLine(view, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            std::string id = trim(line.substr(1, line.size() - 2));
            if (id.rfind("node ", 0) == 0) id = trim(id.substr(5));
            nodes_.push_back(StoryNode{});
            cur = &nodes_.back();
            cur->id = id;
            continue;
        }
        if (!cur) continue;
        const auto sp = line.find(' ');
        const std::string key = (sp == std::string::npos) ? line : line.substr(0, sp);
        const std::string val = (sp == std::string::npos) ? std::string{} : trim(line.substr(sp + 1));
        if (key == "prompt_zh") cur->promptZh = val;
        else if (key == "prompt_en") cur->promptEn = val;
        else if (key == "speaker_zh") cur->speakerZh = val;
        else if (key == "speaker_en") cur->speakerEn = val;
        else if (key == "color") cur->color = static_cast<std::uint8_t>(std::clamp(std::stoi(val), 0, 15));
        else if (key == "text_zh") cur->linesZh.push_back(val);
        else if (key == "text_en") cur->linesEn.push_back(val);
        else if (key == "mad_zh") cur->madZh = val;
        else if (key == "mad_en") cur->madEn = val;
        else if (key == "on_enter") {
            const char* p = val.c_str();
            int f1 = 0, f2 = 0, endId = 0;
            scanFloat(p, cur->onEnterSanity);
            scanInt(p, f1);
            scanInt(p, f2);
            scanInt(p, endId);
            cur->onEnterFlag = static_cast<std::uint8_t>(std::clamp(f1, 0, 63));
            cur->onEnterFlag2 = static_cast<std::uint8_t>(std::clamp(f2, 0, 63));
            cur->onEnterEnding = static_cast<std::uint8_t>(std::clamp(endId, 0, 3));
        } else if (key == "choice") {
            // Format: choice <targetId> <sanDelta> <setFlag> <setFlag2> <ending> <zhText> | <enText>
            const char* p = val.c_str();
            StoryChoice ch{};
            int f1 = 0, f2 = 0, endId = 0;
            if (scanToken(p, ch.targetId) && scanFloat(p, ch.sanityDelta) &&
                scanInt(p, f1) && scanInt(p, f2) && scanInt(p, endId)) {
                ch.setFlag = static_cast<std::uint8_t>(std::clamp(f1, 0, 63));
                ch.setFlag2 = static_cast<std::uint8_t>(std::clamp(f2, 0, 63));
                ch.ending = static_cast<std::uint8_t>(std::clamp(endId, 0, 3));
                std::string rest = trim(p);
                const auto bar = rest.find('|');
                if (bar != std::string::npos) {
                    ch.textZh = trim(rest.substr(0, bar));
                    ch.textEn = trim(rest.substr(bar + 1));
                } else {
                    ch.textZh = ch.textEn = rest;
                }
                cur->choices.push_back(std::move(ch));
            }
        }
    }
    return !nodes_.empty();
}

void StoryRuntime::reset() {
    const bool keepZh = state_.zh;
    state_ = StoryState{};
    state_.zh = keepZh;
    nearbyNode_ = -1;
}

int StoryRuntime::findNode(const std::string& id) const {
    if (id.empty() || id == "end") return -1;
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

void StoryRuntime::updateAllSealsFlag() {
    // Story convention: flags 3 (West Seal), 4 (East Seal), 5 (North Seal) -> automatically set flag 6 (All Seals)!
    if (hasFlag(3) && hasFlag(4) && hasFlag(5)) setFlag(6);
    // Secret Star-Gate unlocks when Hidden Log (flag 7) + Echo Forgiven (flag 8) + All Seals (flag 6) are active -> flag 9!
    if (hasFlag(6) && hasFlag(7) && hasFlag(8)) setFlag(9);
}

void StoryRuntime::applyEffects(float sanDelta, std::uint8_t f1, std::uint8_t f2, std::uint8_t ending) {
    state_.sanity = std::clamp(state_.sanity + sanDelta, 5.0f, 100.0f);
    if (f1) setFlag(f1);
    if (f2) setFlag(f2);
    updateAllSealsFlag();
    if (ending > 0) {
        state_.ending = ending;
        state_.freeRoamAfterEnding = false;
    }
}

void StoryRuntime::openNode(int idx) {
    if (idx < 0 || idx >= static_cast<int>(nodes_.size())) {
        state_.activeNode = -1;
        return;
    }
    state_.activeNode = static_cast<std::int16_t>(idx);
    state_.page = 0;
    state_.choiceCursor = 0;
    state_.revealTimer = 0.0f;
    const auto& n = nodes_[static_cast<std::size_t>(idx)];
    applyEffects(n.onEnterSanity, n.onEnterFlag, n.onEnterFlag2, n.onEnterEnding);
}

int StoryRuntime::skyOverride() const {
    if (state_.ending == 1) return 1; // Calm starry night restored
    if (state_.ending == 2) return 2; // Permanent eldritch eclipse
    return 0;
}

double StoryRuntime::horizonSway(double sceneTime) const {
    if (!loaded() || state_.sanity >= 42.0f) return 0.0;
    const double intensity = (42.0 - state_.sanity) / 42.0;
    return std::sin(sceneTime * 2.1) * intensity * 2.8;
}

std::string StoryRuntime::currentText() const {
    const auto* n = currentNode();
    if (!n) return {};
    const auto& lines = state_.zh ? n->linesZh : n->linesEn;
    if (state_.page == 0 && state_.sanity < 45.0f) {
        const std::string& mad = state_.zh ? n->madZh : n->madEn;
        if (!mad.empty()) return mad;
    }
    if (state_.page < lines.size()) return lines[state_.page];
    return lines.empty() ? std::string{} : lines.back();
}

std::size_t StoryRuntime::revealedBytes() const {
    const std::string txt = currentText();
    const std::size_t maxChars = static_cast<std::size_t>(state_.revealTimer * 44.0f) + 1;
    return utf8PrefixBytes(txt, maxChars);
}

bool StoryRuntime::pageFullyRevealed() const {
    const std::string txt = currentText();
    return revealedBytes() >= txt.size();
}

bool StoryRuntime::hasChoicesOnCurrentPage() const {
    const auto* n = currentNode();
    if (!n || n->choices.empty()) return false;
    const auto& lines = state_.zh ? n->linesZh : n->linesEn;
    const std::size_t totalPages = std::max<std::size_t>(1, lines.size());
    return state_.page + 1 >= totalPages;
}

void StoryRuntime::update(const InputState& input, WorldMap& map, const PlayerPose& pose, double dt) {
    if (input.toggleLang) toggleLanguage();
    if (!loaded()) return;

    // 1. Update world gates, zone atmosphere, and active compass beacon every frame.
    map.updateStoryWorld(dt, state_.flags, pose.x, pose.y, skyOverride());

    // 2. Ending card input: Space / Enter / E continues into free roam!
    if (showingEnding()) {
        if (input.menuConfirm || input.interact || input.jump) {
            state_.freeRoamAfterEnding = true;
            state_.activeNode = -1;
        }
        return;
    }

    // 3. Active dialogue progression & branching choices.
    if (active()) {
        state_.revealTimer += static_cast<float>(dt);
        const auto* n = currentNode();
        const auto& lines = state_.zh ? n->linesZh : n->linesEn;
        const std::size_t totalPages = std::max<std::size_t>(1, lines.size());
        const bool onChoicePage = hasChoicesOnCurrentPage();

        if (onChoicePage && pageFullyRevealed()) {
            const int cnt = static_cast<int>(n->choices.size());
            if (input.menuUp) state_.choiceCursor = static_cast<std::uint8_t>((state_.choiceCursor + cnt - 1) % cnt);
            if (input.menuDown) state_.choiceCursor = static_cast<std::uint8_t>((state_.choiceCursor + 1) % cnt);
            int picked = -1;
            if (input.choice1 && cnt >= 1) picked = 0;
            else if (input.choice2 && cnt >= 2) picked = 1;
            else if (input.menuConfirm || input.interact) picked = state_.choiceCursor;
            if (picked >= 0 && picked < cnt) {
                const auto ch = n->choices[static_cast<std::size_t>(picked)];
                applyEffects(ch.sanityDelta, ch.setFlag, ch.setFlag2, ch.ending);
                openNode(findNode(ch.targetId));
            }
            return;
        }

        if (input.interact || input.menuConfirm || input.jump) {
            if (!pageFullyRevealed()) {
                state_.revealTimer = static_cast<float>(utf8CharCount(currentText()) + 5);
            } else if (state_.page + 1 < totalPages) {
                ++state_.page;
                state_.revealTimer = 0.0f;
            } else if (!onChoicePage) {
                state_.activeNode = -1;
            }
        }
        return;
    }

    // 4. Real gameplay Sanity dynamics: gazing into eldritch sky/beacon drains gently; calm Lighthouse restores.
    if (map.sky().eldritch) {
        const vec2 beacon = map.beaconPosition();
        const double toB = std::atan2(beacon.y - pose.y, beacon.x - pose.x);
        const double diff = std::atan2(std::sin(toB - pose.yaw), std::cos(toB - pose.yaw));
        const double gaze = std::max(0.0, std::cos(diff));
        const double drain = 0.35 + 1.15 * gaze;
        state_.sanity = std::clamp(state_.sanity - static_cast<float>(drain * dt), 12.0f, 100.0f);
    } else {
        state_.sanity = std::clamp(state_.sanity + static_cast<float>(4.5 * dt), 12.0f, 100.0f);
    }

    // 5. Scan world triggers (nearest interactable + automatic enter triggers).
    nearbyNode_ = -1;
    double bestDistSq = 1e9;
    for (const auto& tr : map.triggers()) {
        if (!hasFlag(tr.reqFlag)) continue;
        if (tr.forbidFlag != 0 && hasFlag(tr.forbidFlag)) continue;
        const double dx = pose.x - tr.x;
        const double dy = pose.y - tr.y;
        const double dSq = dx * dx + dy * dy;
        if (dSq > static_cast<double>(tr.radius) * tr.radius) continue;
        const int nodeIdx = findNode(map.triggerEventName(tr.eventIndex));
        if (nodeIdx < 0) continue;
        if (tr.kind == 1) {
            // Auto enter trigger
            openNode(nodeIdx);
            return;
        }
        if (dSq < bestDistSq) {
            bestDistSq = dSq;
            nearbyNode_ = nodeIdx;
        }
    }

    if (nearbyNode_ >= 0 && input.interact) {
        openNode(nearbyNode_);
    }
}

std::string StoryRuntime::promptLabel() const {
    if (nearbyNode_ < 0 || active() || showingEnding()) return {};
    const auto& n = nodes_[static_cast<std::size_t>(nearbyNode_)];
    const std::string& p = state_.zh ? n.promptZh : n.promptEn;
    if (p.empty()) return state_.zh ? "[E] 调查" : "[E] INSPECT";
    return (state_.zh ? "[E] " : "[E] ") + p;
}

std::string StoryRuntime::objectiveLabel() const {
    if (!loaded()) return {};
    if (state_.ending == 1) return state_.zh ? "结局 A：永夜守灯人（自由探索）" : "ENDING A: KEEPER OF LIGHT (FREE ROAM)";
    if (state_.ending == 2) return state_.zh ? "结局 B：黑星目击者（自由探索）" : "ENDING B: WITNESS OF BLACK STAR";
    if (state_.ending == 3) return state_.zh ? "隐藏结局 C：深渊同行" : "SECRET ENDING C: INTO THE STAR-GATE";
    if (!hasFlag(1)) return state_.zh ? "目标：探索灯塔并点燃码头信号火盆" : "OBJ: EXPLORE LIGHTHOUSE & LIGHT DOCK BRAZIER";
    if (!hasFlag(2)) return state_.zh ? "目标：穿过沉降的海蚀巨门，步入退潮海沟" : "OBJ: DESCEND THROUGH THE SUNKEN SEA-GATE";
    if (!hasFlag(6)) {
        const int seals = (hasFlag(3) ? 1 : 0) + (hasFlag(4) ? 1 : 0) + (hasFlag(5) ? 1 : 0);
        if (state_.zh) return "目标：重燃三翼旧印封印 (" + std::to_string(seals) + "/3)";
        return "OBJ: REKINDLE THREE ELDER SEALS (" + std::to_string(seals) + "/3)";
    }
    if (hasFlag(9)) {
        return state_.zh ? "目标：前往中央祭坛，或步入北翼觉醒的星门" : "OBJ: CONFRONT CENTRAL ALTAR OR NORTH STAR-GATE";
    }
    return state_.zh ? "目标：三封印已燃，返回中央祭坛直面沉眠者" : "OBJ: RETURN TO CENTRAL ALTAR TO FACE THE SLEEPER";
}
