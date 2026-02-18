#include "WorldEditMod.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Command.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/BlockChangeContext.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/chrono/GameChrono.h"
#include "mc/dataloadhelper/NewUniqueIdsDataLoadHelper.h"
#include "mc/world/level/Level.h"
#include <chrono>
#include <string>

namespace my_mod {

ll::coro::CoroTask<void> executeRedoTask(Player* player, EditAction action) {
    auto& region = player->getDimension().getBlockSourceFromMainChunkSource();
    BlockChangeContext context(true);
    
    std::vector<BlockEdit> undoList;
    undoList.reserve(action.blocks.size());

    auto startTime = std::chrono::steady_clock::now();
    auto measureStart = std::chrono::high_resolution_clock::now();
    const auto timeBudget = std::chrono::milliseconds(30);

    for (auto it = action.blocks.rbegin(); it != action.blocks.rend(); ++it) {
        if (it->dim != player->getDimensionId()) continue;

        const Block& currentBlock = region.getBlock(it->pos);
        std::unique_ptr<CompoundTag> currentNbt = nullptr;
        if (auto* actor = region.getBlockEntity(it->pos)) {
            currentNbt = std::make_unique<CompoundTag>();
            actor->saveBlockData(*currentNbt, region);
        }

        undoList.push_back({it->pos, &currentBlock, std::move(currentNbt), it->dim});

        region.setBlock(it->pos, *it->oldBlock, 3, nullptr, context);

        if (it->oldNbt) {
            if (auto* actor = region.getBlockEntity(it->pos)) {
                NewUniqueIdsDataLoadHelper helper;
                helper.mLevel = &player->getLevel();
                actor->loadBlockData(*it->oldNbt, region, helper);
                actor->onChanged(region);
                actor->refresh(region);
            }
        }

        if (std::chrono::steady_clock::now() - startTime >= timeBudget) {
            co_await ll::chrono::ticks(1);
            startTime = std::chrono::steady_clock::now();
        }
    }

    WorldEditMod::getInstance().getSessionManager().pushHistory(*player, {std::move(undoList)});
    
    auto measureEnd = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(measureEnd - measureStart).count();

    player->sendMessage("§aRedo completed successfully. Time taken: " + std::to_string(duration) + "ms");
    co_return;
}

void registerRedoCommand() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);
    auto& redoCmd = registrar.getOrCreateCommand("redo", "Redo last operation");

    redoCmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (!entity || !entity->isType(ActorType::Player)) {
            output.error("Only players can use this command");
            return;
        }

        auto* player = static_cast<Player*>(entity);
        if (!player->isOperator()) {
            output.error("You do not have permission to use this command.");
            return;
        }

        auto actionOpt = WorldEditMod::getInstance().getSessionManager().popRedo(*player);

        if (actionOpt.has_value()) {
            executeRedoTask(player, std::move(actionOpt.value())).launch(ll::thread::ServerThreadExecutor::getDefault());
            output.success("§aRedo operation started...");
        } else {
            output.error("Nothing to redo.");
        }
    });
}

}

