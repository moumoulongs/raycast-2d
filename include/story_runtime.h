#pragma once

#include "input.h"
#include "player_pose.h"
#include "world_map.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct StoryChoice {
    std::string targetId;
    std::string textZh;
    std::string textEn;
    float sanityDelta = 0.0f;
    std::uint8_t setFlag = 0;
    std::uint8_t setFlag2 = 0;
    std::uint8_t ending = 0; // 0=none, 1=Seal, 2=Madness, 3=Secret
};

struct StoryNode {
    std::string id;
    std::string promptZh;
    std::string promptEn;
    std::string speakerZh;
    std::string speakerEn;
    std::vector<std::string> linesZh;
    std::vector<std::string> linesEn;
    std::string madZh; // Unreliable narrator override when sanity < 45
    std::string madEn;
    std::vector<StoryChoice> choices;
    float onEnterSanity = 0.0f;
    std::uint8_t color = 14;
    std::uint8_t onEnterFlag = 0;
    std::uint8_t onEnterFlag2 = 0;
    std::uint8_t onEnterEnding = 0;
    std::uint8_t reqAllFlagsMask = 0; // optional helper
};

struct StoryState {
    std::uint64_t flags = 0;
    float sanity = 100.0f;
    float revealTimer = 0.0f;
    std::int16_t activeNode = -1;
    std::uint8_t page = 0;
    std::uint8_t choiceCursor = 0;
    std::uint8_t ending = 0; // 0=playing, 1=Ending A, 2=Ending B, 3=Ending C
    bool zh = true;          // true=Chinese, false=English
    bool freeRoamAfterEnding = false;
    std::uint8_t pad = 0;
};
static_assert(sizeof(StoryState) <= 32, "StoryState must remain compact (<= 32 bytes)");

class StoryRuntime {
public:
    bool load(const std::filesystem::path& storyPath);
    void reset();

    bool loaded() const { return !nodes_.empty(); }
    bool active() const { return state_.activeNode >= 0; }
    bool showingEnding() const { return state_.ending != 0 && !state_.freeRoamAfterEnding; }
    bool blocksMovement() const { return active() || showingEnding(); }

    void toggleLanguage() { state_.zh = !state_.zh; }
    bool isZh() const { return state_.zh; }

    std::uint64_t flags() const { return state_.flags; }
    float sanity() const { return state_.sanity; }
    std::uint8_t ending() const { return state_.ending; }
    int skyOverride() const;
    double horizonSway(double sceneTime) const;

    void update(const InputState& input, WorldMap& map, const PlayerPose& pose, double dt);

    // UI query helpers
    std::string promptLabel() const;
    std::string objectiveLabel() const;
    const StoryNode* currentNode() const {
        return state_.activeNode >= 0 ? &nodes_[static_cast<std::size_t>(state_.activeNode)] : nullptr;
    }
    std::string currentText() const;
    std::size_t revealedBytes() const;
    bool pageFullyRevealed() const;
    std::uint8_t choiceCursor() const { return state_.choiceCursor; }
    bool hasChoicesOnCurrentPage() const;

private:
    int findNode(const std::string& id) const;
    void openNode(int idx);
    void applyEffects(float sanDelta, std::uint8_t f1, std::uint8_t f2, std::uint8_t ending);
    bool hasFlag(std::uint8_t bitNum) const {
        return bitNum == 0 || ((state_.flags & (std::uint64_t{1} << (bitNum - 1))) != 0);
    }
    void setFlag(std::uint8_t bitNum) {
        if (bitNum >= 1 && bitNum <= 63) state_.flags |= (std::uint64_t{1} << (bitNum - 1));
    }
    void updateAllSealsFlag();

    std::vector<StoryNode> nodes_;
    StoryState state_{};
    int nearbyNode_ = -1;
};
