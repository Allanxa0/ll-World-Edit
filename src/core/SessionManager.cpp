#include "SessionManager.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/BlockSource.h"

namespace my_mod {

SessionManager& SessionManager::getInstance() {
    static SessionManager instance;
    return instance;
}

Selection& SessionManager::getSelection(Player& player) {
    return mSessions[player.getXuid()].selection;
}

bool SessionManager::canUseWand(Player& player) {
    auto& session = mSessions[player.getXuid()];
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - session.lastWandUse).count();
    return elapsed >= WAND_COOLDOWN_MS;
}

void SessionManager::updateWandUsage(Player& player) {
    mSessions[player.getXuid()].lastWandUse = std::chrono::steady_clock::now();
}

void SessionManager::setPos1(Player& player, const BlockPos& pos) {
    auto& sel = mSessions[player.getXuid()].selection;
    sel.pos1 = pos;
    sel.dimId = player.getDimensionId();
}

void SessionManager::setPos2(Player& player, const BlockPos& pos) {
    auto& sel = mSessions[player.getXuid()].selection;
    sel.pos2 = pos;
    sel.dimId = player.getDimensionId();
}

void SessionManager::pushHistory(Player& player, EditAction&& action) {
    auto& history = mUndoHistory[player.getXuid()];
    history.push_back(std::move(action));
    if (history.size() > MAX_HISTORY_SIZE) {
        history.pop_front();
    }
    mRedoHistory[player.getXuid()].clear();
}

std::optional<EditAction> SessionManager::popUndo(Player& player) {
    auto& history = mUndoHistory[player.getXuid()];
    if (history.empty()) return std::nullopt;

    auto action = std::move(history.back());
    history.pop_back();
    return action;
}

void SessionManager::pushRedo(Player& player, EditAction&& action) {
    auto& history = mRedoHistory[player.getXuid()];
    history.push_back(std::move(action));
    if (history.size() > MAX_HISTORY_SIZE) {
        history.pop_front();
    }
}

std::optional<EditAction> SessionManager::popRedo(Player& player) {
    auto& history = mRedoHistory[player.getXuid()];
    if (history.empty()) return std::nullopt;

    auto action = std::move(history.back());
    history.pop_back();
    return action;
}

}

