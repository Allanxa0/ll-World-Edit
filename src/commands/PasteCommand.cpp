#include "WorldEditMod.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Command.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/ChunkPos.h"
#include "mc/world/level/block/BlockChangeContext.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/item/SaveContext.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/service/Bedrock.h"
#include "mc/dataloadhelper/NewUniqueIdsDataLoadHelper.h"
#include "mc/world/level/Level.h"
#include <map>

namespace my_mod {

ll::coro::CoroTask<void> executePasteTask(std::string xuid, BlockPos playerPos, DimensionType dim) {
    auto playerOpt = ll::service::getLevel()->getPlayerByXuid(xuid);
    if (!playerOpt) co_return;
    Player* player = playerOpt;

    auto& clipboard = WorldEditMod::getInstance().getSessionManager().getClipboard(*player);
    if (clipboard.empty()) {
        player->sendMessage("§cClipboard is empty.");
        co_return;
    }

    if (clipboard.size() > WorldEditMod::getInstance().getConfig().maxBlocksPerOperation) {
        player->sendMessage("§cPaste exceeds maximum block limit.");
        co_return;
    }

    auto& region = player->getDimension().getBlockSourceFromMainChunkSource();
    BlockChangeContext context(true); 

    std::map<ChunkPos, std::vector<const ClipboardItem*>> chunkBatches;
    for (const auto& item : clipboard) {
        BlockPos targetPos = playerPos + item.offset;
        chunkBatches[ChunkPos(targetPos.x >> 4, targetPos.z >> 4)].push_back(&item);
    }

    std::vector<BlockEdit> undoHistory;
    int configBlocks = WorldEditMod::getInstance().getConfig().blocksPerTick;
    int configChunks = WorldEditMod::getInstance().getConfig().chunksPerTick;

    int blocksProcessed = 0;
    int chunksProcessed = 0;
    int appliedCount = 0;

    for (const auto& [chunkPos, items] : chunkBatches) {
        if (!region.hasChunksAt(playerPos + items.front()->offset, 0, false)) continue;

        for (const auto* item : items) {
            BlockPos targetPos = playerPos + item->offset;
            const Block& oldBlock = region.getBlock(targetPos);
            
            if (oldBlock == *item->block && !item->nbt && !region.getBlockEntity(targetPos)) {
                continue;
            }

            std::unique_ptr<CompoundTag> oldNbt = nullptr;
            if (auto* actor = region.getBlockEntity(targetPos)) {
                oldNbt = std::make_unique<CompoundTag>();
                actor->save(*oldNbt, SaveContext());
                region.removeBlockEntity(targetPos);
            }

            undoHistory.push_back({targetPos, &oldBlock, std::move(oldNbt), dim});
            region.setBlock(targetPos, *item->block, 3, nullptr, context);

            if (item->nbt) {
                if (auto* actor = region.getBlockEntity(targetPos)) {
                    NewUniqueIdsDataLoadHelper helper;
                    helper.mLevel = &player->getLevel();
                    
                    auto clonedNbt = std::make_unique<CompoundTag>(*item->nbt);
                    (*clonedNbt)["x"] = targetPos.x;
                    (*clonedNbt)["y"] = targetPos.y;
                    (*clonedNbt)["z"] = targetPos.z;

                    actor->load(player->getLevel(), *clonedNbt, helper);
                    actor->onChanged(region);
                    actor->refresh(region);
                }
            }

            appliedCount++;
            blocksProcessed++;
            if (blocksProcessed >= configBlocks) {
                co_await ll::chrono::ticks(1);
                blocksProcessed = 0;
                chunksProcessed = 0;
            }
        }
        
        chunksProcessed++;
        if (chunksProcessed >= configChunks) {
            co_await ll::chrono::ticks(1);
            chunksProcessed = 0;
        }
    }

    if (auto currentP = ll::service::getLevel()->getPlayerByXuid(xuid)) {
        WorldEditMod::getInstance().getSessionManager().pushHistory(*currentP, {std::move(undoHistory)});
        currentP->sendMessage("§aPasted " + std::to_string(appliedCount) + " blocks.");
    }
    co_return;
}

void registerPasteCommand() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);
    auto& pasteCmd = registrar.getOrCreateCommand("paste", "Paste clipboard");

    pasteCmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (!entity || !entity->isType(ActorType::Player)) return;
        auto* player = static_cast<Player*>(entity);

        executePasteTask(player->getXuid(), player->getPosition(), player->getDimensionId())
            .launch(ll::thread::ServerThreadExecutor::getDefault());
        output.success("Paste operation started...");
    });
}

}



