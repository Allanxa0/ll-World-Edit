#pragma once
#include "Selection.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/block/Block.h"
#include "mc/nbt/CompoundTag.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <optional>
#include <chrono>

namespace my_mod {

struct BlockEdit {
    BlockPos pos;
    const Block* oldBlock;
    std::unique_ptr<CompoundTag> oldNbt;
    DimensionType dim;
};

struct EditAction {
    std::vector<BlockEdit> blocks;
};

struct PlayerSession {
    Selection selection;
    std::chrono::steady_clock::time_point lastWandUse;
};

class SessionManager {
public:
    static SessionManager& getInstance(); 

    Selection& getSelection(Player& player);
    bool canUseWand(Player& player);
    void updateWandUsage(Player& player);
    void setPos1(Player& player, const BlockPos& pos);
    void setPos2(Player& player, const BlockPos& pos);
    
    void pushHistory(Player& player, EditAction&& action);
    std::optional<EditAction> popUndo(Player& player);
    void pushRedo(Player& player, EditAction&& action);
    std::optional<EditAction> popRedo(Player& player);

    void updateSelectionVisuals(Player& player);
    void clearSelectionVisuals(Player& player);
    void onPlayerLeft(Player& player);

private:
    std::unordered_map<std::string, PlayerSession> mSessions;
    std::unordered_map<std::string, std::deque<EditAction>> mUndoHistory;
    std::unordered_map<std::string, std::deque<EditAction>> mRedoHistory;
    std::unordered_map<std::string, std::vector<BlockPos>> mVisualBlocks;

    static constexpr int MAX_HISTORY_SIZE = 10;
    static constexpr int WAND_COOLDOWN_MS = 400;
};

}

