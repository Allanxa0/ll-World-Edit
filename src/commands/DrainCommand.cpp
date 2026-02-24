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
#include "mc/world/level/Level.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/deps/core/string/HashedString.h"
#include <string_view>
#include <map>

namespace my_mod {

struct DrainParams {
    int radius;
};

ll::coro::CoroTask<void> executeDrainTask(std::string xuid, BlockPos center, int radius, DimensionType dim) {
    auto levelOpt = ll::service::getLevel();
    if (!levelOpt.has_value()) co_return;
    Player* player = levelOpt->getPlayerByXuid(xuid);
    if (!player) co_return;

    int r = std::min(radius, 50);
    auto& region = player->getDimension().getBlockSourceFromMainChunkSource();
    BlockChangeContext context(true);

    auto airBlockOpt = Block::tryGetFromRegistry(HashedString("minecraft:air"));
    if (!airBlockOpt) co_return;
    const Block& airBlock = airBlockOpt.value();

    std::map<ChunkPos, std::vector<BlockPos>> chunkBatches;
    
    for (int x = -r; x <= r; ++x) {
        for (int y = -r; y <= r; ++y) {
            for (int z = -r; z <= r; ++z) {
                if (x*x + y*y + z*z <= r*r) {
                    BlockPos p(center.x + x, center.y + y, center.z + z);
                    chunkBatches[ChunkPos(p.x >> 4, p.z >> 4)].emplace_back(p);
                }
            }
        }
    }

    std::vector<BlockEdit> undoHistory;
    int configBlocks = WorldEditMod::getInstance().getConfig().blocksPerTick;
    int configChunks = WorldEditMod::getInstance().getConfig().chunksPerTick;

    int blocksProcessed = 0;
    int chunksProcessed = 0;
    int replacedCount = 0;

    for (const auto& [chunkPos, blocks] : chunkBatches) {
        if (!region.hasChunksAt(blocks.front(), 0, false)) continue;

        for (const auto& targetPos : blocks) {
            const Block& oldBlock = region.getBlock(targetPos);
            const std::string& typeName = oldBlock.getTypeName();
            
            if (typeName == "minecraft:water" || typeName == "minecraft:flowing_water" || typeName == "minecraft:lava" || typeName == "minecraft:flowing_lava") {
                std::unique_ptr<CompoundTag> oldNbt = nullptr;
                if (auto* actor = region.getBlockEntity(targetPos)) {
                    oldNbt = std::make_unique<CompoundTag>();
                    actor->save(*oldNbt, SaveContext());
                    region.removeBlockEntity(targetPos);
                }

                undoHistory.push_back({targetPos, &oldBlock, std::move(oldNbt), dim});
                region.setBlock(targetPos, airBlock, 3, nullptr, context);
                replacedCount++;
            }

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

    auto currentLevelOpt = ll::service::getLevel();
    if (currentLevelOpt.has_value()) {
        if (auto currentP = currentLevelOpt->getPlayerByXuid(xuid)) {
            WorldEditMod::getInstance().getSessionManager().pushHistory(*currentP, {std::move(undoHistory)});
            currentP->sendMessage("§aDrained " + std::to_string(replacedCount) + " liquid blocks.");
        }
    }
    co_return;
}

void registerDrainCommand() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);
    auto& drainCmd = registrar.getOrCreateCommand("drain", "Drain nearby fluids");

    drainCmd.overload<DrainParams>()
        .required("radius")
        .execute([](CommandOrigin const& origin, CommandOutput& output, DrainParams const& params) {
            auto* entity = origin.getEntity();
            if (!entity || !entity->isType(ActorType::Player)) return;
            auto* player = static_cast<Player*>(entity);

            executeDrainTask(player->getXuid(), player->getPosition(), params.radius, player->getDimensionId())
                .launch(ll::thread::ServerThreadExecutor::getDefault());
            output.success("Drain operation started...");
        });
}

}



