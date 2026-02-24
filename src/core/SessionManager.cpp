#include "SessionManager.h"
#include "WorldEditMod.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/BlockSource.h"
#include "mc/network/packet/UpdateBlockPacket.h"
#include "mc/network/packet/BlockActorDataPacket.h"
#include "mc/world/level/block/Block.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/network/NetworkBlockPosition.h"
#include "mc/nbt/CompoundTag.h"
#include "mc/world/level/ChunkPos.h"
#include "mc/deps/core/math/Vec3.h"
#include <algorithm>
#include <cmath>

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
    return elapsed >= WorldEditMod::getInstance().getConfig().wandCooldownMs;
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
    if (action.blocks.empty()) return;
    auto& history = mUndoHistory[player.getXuid()];
    history.push_back(std::move(action));
    if (history.size() > WorldEditMod::getInstance().getConfig().maxHistorySize) {
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
    if (action.blocks.empty()) return;
    auto& history = mRedoHistory[player.getXuid()];
    history.push_back(std::move(action));
    if (history.size() > WorldEditMod::getInstance().getConfig().maxHistorySize) {
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

void SessionManager::setClipboard(Player& player, std::vector<ClipboardItem>&& clipboard) {
    mClipboards[player.getXuid()] = std::move(clipboard);
}

std::vector<ClipboardItem>& SessionManager::getClipboard(Player& player) {
    return mClipboards[player.getXuid()];
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
    int hideY = std::max(-64, minY - 1);
    BlockPos pos(minX, hideY, minZ);
    auto visualBlockOpt = Block::tryGetFromRegistry(HashedString("minecraft:structure_block"));
    if (!visualBlockOpt) return;
    std::vector<BlockPos>& visuals = mVisualBlocks[player.getXuid()];
    visuals.push_back(pos);
    UpdateBlockPacket packet;
    packet.mPos = NetworkBlockPosition(pos);
    packet.mLayer = 0;
    packet.mUpdateFlags = 3;
    packet.mRuntimeId = visualBlockOpt->mNetworkId;
    player.sendNetworkPacket(packet);
    CompoundTag tag;
    tag["id"] = std::string("StructureBlock");
    tag["x"] = pos.x;
    tag["y"] = pos.y;
    tag["z"] = pos.z;
    tag["xStructureSize"] = maxX - minX + 1;
    tag["yStructureSize"] = maxY - minY + 1;
    tag["zStructureSize"] = maxZ - minZ + 1;
    tag["xStructureOffset"] = 0;
    tag["yStructureOffset"] = minY - hideY;
    tag["zStructureOffset"] = 0;
    tag["showBoundingBox"] = static_cast<unsigned char>(1);
    tag["structureName"] = std::string("we_selection");
    BlockActorDataPacket dataPacket(BlockActorDataPacketPayload(pos, std::move(tag)));
    player.sendNetworkPacket(dataPacket);
}

void SessionManager::checkAndResendVisuals(Player& player, const Vec3& pos) {
    ChunkPos currentChunk(static_cast<int>(std::floor(pos.x)) >> 4, static_cast<int>(std::floor(pos.z)) >> 4);
    auto& session = mSessions[player.getXuid()];
    if (session.lastChunkPos.x != currentChunk.x || session.lastChunkPos.z != currentChunk.z) {
        session.lastChunkPos = currentChunk;
        if (session.selection.isComplete()) {
            updateSelectionVisuals(player);
        }
    }
}

void SessionManager::onPlayerLeft(Player& player) {
    mVisualBlocks.erase(player.getXuid());
    mSessions.erase(player.getXuid());
}

}


