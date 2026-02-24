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
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/chrono/GameChrono.h"
#include <chrono>
#include <unordered_map>
#include <string>

namespace my_mod {

ll::coro::CoroTask<void> executeCountTask(Player* player, BlockPos p1, BlockPos p2) {
    int minX = std::min(p1.x, p2.x);
    int minY = std::min(p1.y, p2.y);
    int minZ = std::min(p1.z, p2.z);
    int maxX = std::max(p1.x, p2.x);
    int maxY = std::max(p1.y, p2.y);
    int maxZ = std::max(p1.z, p2.z);

    auto startTime = std::chrono::steady_clock::now();
    int totalBlocks = 0;
    std::unordered_map<std::string, int> blockCounts;

    auto& region = player->getDimension().getBlockSourceFromMainChunkSource();
    int configBlocks = WorldEditMod::getInstance().getConfig().blocksPerTick;
    int blocksProcessed = 0;

    for (int x = minX; x <= maxX; ++x) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                BlockPos targetPos(x, y, z);
                if (!region.hasChunksAt(targetPos, 0, false)) continue;

                const Block& block = region.getBlock(targetPos);
                blockCounts[block.getTypeName()]++;
                totalBlocks++;
                blocksProcessed++;

                if (blocksProcessed >= configBlocks) {
                    co_await ll::chrono::ticks(1);
                    blocksProcessed = 0;
                }
            }
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::string result = "Operation completed in " + std::to_string(duration) + "ms\n";
    result += "Changed: " + std::to_string(totalBlocks) + " en el área seleccionada\n";
    result += "Tipos de bloques:\n";
    for (const auto& [name, count] : blockCounts) {
        std::string display = name;
        if (display.find("minecraft:") == 0) {
            display = display.substr(10);
        }
        result += display + ": " + std::to_string(count) + "\n";
    }

    player->sendMessage(result);
    co_return;
}

void registerCountCommand() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);
    auto& countCmd = registrar.getOrCreateCommand("count", "Count blocks in selection");

    countCmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (!entity || !entity->isType(ActorType::Player)) return;
        auto* player = static_cast<Player*>(entity);

        auto& session = WorldEditMod::getInstance().getSessionManager().getSelection(*player);
        if (!session.isComplete() || session.dimId.value() != player->getDimensionId()) {
            output.error("Invalid selection.");
            return;
        }

        executeCountTask(player, session.pos1.value(), session.pos2.value())
            .launch(ll::thread::ServerThreadExecutor::getDefault());
        output.success("Count operation started...");
    });
}

}




