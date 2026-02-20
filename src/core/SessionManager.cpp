#include "SessionManager.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/BlockSource.h"
#include "mc/network/packet/UpdateBlockPacket.h"
#include "mc/world/level/block/Block.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/network/NetworkBlockPosition.h"
#include <algorithm>

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
    updateSelectionVisuals(player);
}

void SessionManager::setPos2(Player& player, const BlockPos& pos) {
    auto& sel = mSessions[player.getXuid()].selection;
    sel.pos2 = pos;
    sel.dimId = player.getDimensionId();
    updateSelectionVisuals(player);
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

void SessionManager::clearSelectionVisuals(Player& player) {
    auto it = mVisualBlocks.find(player.getXuid());
    if (it == mVisualBlocks.end()) return;

    auto& region = player.getDimension().getBlockSourceFromMainChunkSource();
    
    for (const auto& pos : it->second) {
        if (!region.hasChunksAt(pos, 0, false)) continue;
        
        const Block& realBlock = region.getBlock(pos);
        UpdateBlockPacket packet;
        
        packet.mPos = NetworkBlockPosition(pos);
        packet.mLayer = 0;
        packet.mUpdateFlags = 3;
        packet.mRuntimeId = realBlock.mNetworkId;
        
        player.sendNetworkPacket(packet);
    }
    
    it->second.clear();
}

void SessionManager::updateSelectionVisuals(Player& player) {
    clearSelectionVisuals(player);

    auto& sel = mSessions[player.getXuid()].selection;
    if (!sel.isComplete() || sel.dimId.value() != player.getDimensionId()) return;

    BlockPos p1 = sel.pos1.value();
    BlockPos p2 = sel.pos2.value();

    int minX = std::min(p1.x, p2.x);
    int minY = std::min(p1.y, p2.y);
    int minZ = std::min(p1.z, p2.z);
    int maxX = std::max(p1.x, p2.x);
    int maxY = std::max(p1.y, p2.y);
    int maxZ = std::max(p1.z, p2.z);

    auto visualBlockOpt = Block::tryGetFromRegistry(HashedString("minecraft:glass"));
    if (!visualBlockOpt) return; 
    
    uint32_t visualRuntimeId = visualBlockOpt->mNetworkId;

    std::vector<BlockPos>& visuals = mVisualBlocks[player.getXuid()];
    
    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                bool isMinX = (x == minX);
                bool isMaxX = (x == maxX);
                bool isMinY = (y == minY);
                bool isMaxY = (y == maxY);
                bool isMinZ = (z == minZ);
                bool isMaxZ = (z == maxZ);

                int edgeCount = (isMinX || isMaxX ? 1 : 0) + (isMinY || isMaxY ? 1 : 0) + (isMinZ || isMaxZ ? 1 : 0);

                if (edgeCount >= 2) {
                    BlockPos pos(x, y, z);
                    visuals.push_back(pos);

                    UpdateBlockPacket packet;
                    packet.mPos = NetworkBlockPosition(pos);
                    packet.mLayer = 0;
                    packet.mUpdateFlags = 3;
                    packet.mRuntimeId = visualRuntimeId;
                    
                    player.sendNetworkPacket(packet);
                }
            }
        }
    }
}

void SessionManager::onPlayerLeft(Player& player) {
    mSessions.erase(player.getXuid());
    mUndoHistory.erase(player.getXuid());
    mRedoHistory.erase(player.getXuid());
    mVisualBlocks.erase(player.getXuid());
}

}
