#include "WorldEditMod.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Command.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/BlockChangeContext.h"
#include "mc/world/level/block/actor/BlockActor.h"
#include "mc/world/item/SaveContext.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/chrono/GameChrono.h"
#include "mc/dataloadhelper/NewUniqueIdsDataLoadHelper.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"
#include <chrono>
#include <string>

namespace my_mod {

ll::coro::CoroTask<void> executeRedoTask(std::string xuid, EditAction action) {
    auto levelOpt = ll::service::getLevel();
    if (!levelOpt) co_return;

    auto* player = levelOpt->getPlayerByXuid(xuid);
    if (!player) co_return;

    auto& region = player->getDimension().getBlockSourceFromMainChunkSource();
    BlockChangeContext context(true);
    
    std::vector<BlockEdit> undoList;
    undoList.reserve(action.blocks.size());

    auto startTime = std::chrono::steady_clock::now();
    const auto timeBudget = std::chrono::milliseconds(25);

    for (auto it = action.blocks.rbegin(); it != action.blocks.rend(); ++it) {
        auto* currentPlayer = levelOpt->getPlayerByXuid(xuid);
        if (!currentPlayer) co_return;

        if (it->dim != currentPlayer->getDimensionId()) continue;

        const Block& currentBlock = region.getBlock(it->pos);
        std::unique_ptr<CompoundTag> currentNbt = nullptr;
        if (auto* actor = region.getBlockEntity(it->pos)) {
            currentNbt = std::make_unique<CompoundTag>();
            actor->save(*currentNbt, SaveContext());
            region.removeBlockEntity(it->pos);
        }

        undoList.push_back({it->pos, &currentBlock, std::move(currentNbt), it->dim});
        region.setBlock(it->pos, *it->oldBlock, 3, nullptr, context);

        if (it->oldNbt) {
            if (auto* actor = region.getBlockEntity(it->pos)) {
                NewUniqueIdsDataLoadHelper helper;
                helper.mLevel = &currentPlayer->getLevel();
                actor->load(currentPlayer->getLevel(), *it->oldNbt, helper);
                actor->onChanged(region);
                actor->refresh(region);
            }
        }

        if (std::chrono::steady_clock::now() - startTime >= timeBudget) {
            co_await ll::chrono::ticks(1);
            startTime = std::chrono::steady_clock::now();
        }
    }

    if (auto* finalPlayer = levelOpt->getPlayerByXuid(xuid)) {
        WorldEditMod::getInstance().getSessionManager().pushHistory(*finalPlayer, {std::move(undoList)});
        finalPlayer->sendMessage("§aRedo completed.");
    }
    co_return;
}

void registerRedoCommand() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);
    auto& redoCmd = registrar.getOrCreateCommand("redo", "Redo last operation");

    redoCmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (!entity || !entity->isType(ActorType::Player)) return;

        auto* player = static_cast<Player*>(entity);
        if (!player->isOperator()) return;

        auto actionOpt = WorldEditMod::getInstance().getSessionManager().popRedo(*player);
        if (actionOpt.has_value()) {
            executeRedoTask(player->getXuid(), std::move(actionOpt.value())).launch(ll::thread::ServerThreadExecutor::getDefault());
            output.success("Redo operation started...");
        }
    });
}

}
