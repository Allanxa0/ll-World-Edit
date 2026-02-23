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
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/item/SaveContext.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/chrono/GameChrono.h"

namespace my_mod {

ll::coro::CoroTask<void> executeCopyTask(Player* player, BlockPos p1, BlockPos p2) {
    int minX = std::min(p1.x, p2.x);
    int minY = std::min(p1.y, p2.y);
    int minZ = std::min(p1.z, p2.z);
    int maxX = std::max(p1.x, p2.x);
    int maxY = std::max(p1.y, p2.y);
    int maxZ = std::max(p1.z, p2.z);

    int count = (maxX - minX + 1) * (maxY - minY + 1) * (maxZ - minZ + 1);
    if (count > WorldEditMod::getInstance().getConfig().maxBlocksPerOperation) {
        player->sendMessage("§cCopy exceeds maximum block limit.");
        co_return;
    }

    auto& region = player->getDimension().getBlockSourceFromMainChunkSource();
    BlockPos playerPos(player->getPosition());
    
    std::vector<ClipboardItem> clipboard;
    clipboard.reserve(count);

    auto startTime = std::chrono::steady_clock::now();
    auto measureStart = std::chrono::high_resolution_clock::now();
    const auto timeBudget = std::chrono::milliseconds(25);

    for (int x = minX; x <= maxX; ++x) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                BlockPos targetPos(x, y, z);
                if (!region.hasChunksAt(targetPos, 0, false)) continue;

                const Block& block = region.getBlock(targetPos);
                
                std::unique_ptr<CompoundTag> nbt = nullptr;
                if (auto* actor = region.getBlockEntity(targetPos)) {
                    nbt = std::make_unique<CompoundTag>();
                    actor->save(*nbt, SaveContext());
                }

                BlockPos offset = targetPos - playerPos;
                clipboard.push_back({offset, &block, std::move(nbt)});

                if (std::chrono::steady_clock::now() - startTime >= timeBudget) {
                    co_await ll::chrono::ticks(1);
                    startTime = std::chrono::steady_clock::now();
                }
            }
        }
    }

    WorldEditMod::getInstance().getSessionManager().setClipboard(*player, std::move(clipboard));
    
    auto measureEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(measureEnd - measureStart).count();

    player->sendMessage("§aCopied " + std::to_string(count) + " blocks. Time taken: " + std::to_string(duration) + "ms");
    co_return;
}

void registerCopyCommand() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);
    auto& copyCmd = registrar.getOrCreateCommand("copy", "Copy selection to clipboard");

    copyCmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (!entity || !entity->isType(ActorType::Player)) return;
        auto* player = static_cast<Player*>(entity);

        if (!player->isOperator()) {
            output.error("Solo los operadores pueden usar este comando.");
            return;
        }

        auto& session = WorldEditMod::getInstance().getSessionManager().getSelection(*player);
        if (!session.isComplete() || session.dimId.value() != player->getDimensionId()) {
            output.error("Invalid selection.");
            return;
        }

        executeCopyTask(player, session.pos1.value(), session.pos2.value())
            .launch(ll::thread::ServerThreadExecutor::getDefault());
        output.success("Copy operation started...");
    });
}

}