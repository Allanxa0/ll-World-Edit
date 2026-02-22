#include "WorldEditMod.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Command.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandBlockName.h"
#include "mc/server/commands/CommandBlockNameResult.h"
#include "mc/server/commands/CommandRawText.h"
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
#include <algorithm>
#include <map>
#include <vector>
#include <random>
#include <sstream>

namespace my_mod {

struct PatternEntry {
    const Block* block;
    double probability;
};

std::vector<PatternEntry> parsePattern(const std::string& input, CommandOutput& output) {
    std::vector<PatternEntry> pattern;
    std::stringstream ss(input);
    std::string segment;
    double totalWeight = 0.0;
    std::vector<std::pair<const Block*, double>> tempWeights;

    while (std::getline(ss, segment, ',')) {
        std::string blockName;
        double weight = 100.0;
        auto colonPos = segment.find(':');
        if (colonPos != std::string::npos && std::isdigit(segment[colonPos + 1])) {
            blockName = segment.substr(0, colonPos);
            try { weight = std::stod(segment.substr(colonPos + 1)); } catch (...) { weight = 100.0; }
        } else {
            blockName = segment;
        }
        if (blockName.find(':') == std::string::npos) blockName = "minecraft:" + blockName;

        auto blockOpt = Block::tryGetFromRegistry(blockName);
        if (!blockOpt) {
            output.error("Invalid block in pattern: " + blockName);
            return {};
        }
        tempWeights.push_back({&(*blockOpt), weight});
        totalWeight += weight;
    }

    if (totalWeight <= 0) return {};
    double currentProb = 0.0;
    for (auto& p : tempWeights) {
        currentProb += (p.second / totalWeight);
        pattern.push_back({p.first, currentProb});
    }
    return pattern;
}

const Block* resolvePattern(const std::vector<PatternEntry>& pattern, std::mt19937& rng) {
    if (pattern.empty()) return nullptr;
    if (pattern.size() == 1) return pattern[0].block;
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double r = dist(rng);
    for (const auto& entry : pattern) {
        if (r <= entry.probability) return entry.block;
    }
    return pattern.back().block;
}

struct SetParamsBlock { CommandBlockName blockName; };
struct SetParamsPattern { CommandRawText patternStr; };

ll::coro::CoroTask<void> executeSetTask(std::string xuid, BlockPos p1, BlockPos p2, std::vector<PatternEntry> pattern, DimensionType dim) {
    auto playerOpt = ll::service::getLevel()->getPlayerByXuid(xuid);
    if (!playerOpt) co_return;
    Player* player = playerOpt;

    int minX = std::min(p1.x, p2.x); int minY = std::min(p1.y, p2.y); int minZ = std::min(p1.z, p2.z);
    int maxX = std::max(p1.x, p2.x); int maxY = std::max(p1.y, p2.y); int maxZ = std::max(p1.z, p2.z);

    int count = (maxX - minX + 1) * (maxY - minY + 1) * (maxZ - minZ + 1);
    if (count > WorldEditMod::getInstance().getConfig().maxBlocksPerOperation) {
        player->sendMessage("§cOperation exceeds maximum block limit.");
        co_return;
    }

    auto& region = player->getDimension().getBlockSourceFromMainChunkSource();
    BlockChangeContext context(true); 
    
    std::map<ChunkPos, std::vector<BlockPos>> chunkBatches;
    for (int x = minX; x <= maxX; ++x) {
        for (int z = minZ; z <= maxZ; ++z) {
            ChunkPos cp(x >> 4, z >> 4);
            for (int y = minY; y <= maxY; ++y) chunkBatches[cp].emplace_back(x, y, z);
        }
    }

    std::vector<BlockEdit> undoHistory;
    std::mt19937 rng(std::random_device{}());

    int configBlocks = WorldEditMod::getInstance().getConfig().blocksPerTick;
    int configChunks = WorldEditMod::getInstance().getConfig().chunksPerTick;

    int blocksProcessed = 0;
    int chunksProcessed = 0;
    int appliedCount = 0;

    for (const auto& [chunkPos, blocks] : chunkBatches) {
        if (!region.hasChunksAt(blocks.front(), 0, false)) continue;

        for (const auto& targetPos : blocks) {
            const Block& oldBlock = region.getBlock(targetPos);
            const Block* blockToSet = resolvePattern(pattern, rng);
            if (!blockToSet) blockToSet = pattern[0].block;

            if (oldBlock == *blockToSet && !region.getBlockEntity(targetPos)) {
                continue;
            }
            
            std::unique_ptr<CompoundTag> oldNbt = nullptr;
            if (auto* actor = region.getBlockEntity(targetPos)) {
                oldNbt = std::make_unique<CompoundTag>();
                actor->save(*oldNbt, SaveContext());
                region.removeBlockEntity(targetPos);
            }

            undoHistory.push_back({targetPos, &oldBlock, std::move(oldNbt), dim});
            region.setBlock(targetPos, *blockToSet, 3, nullptr, context);
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
        currentP->sendMessage("§aOperation completed. " + std::to_string(appliedCount) + " blocks affected.");
    }
    co_return;
}

void registerSetCommand() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);
    auto& setCmd = registrar.getOrCreateCommand("set", "Set blocks in selection");

    setCmd.overload<SetParamsBlock>()
        .required("blockName")
        .execute([](CommandOrigin const& origin, CommandOutput& output, SetParamsBlock const& params) {
            auto* entity = origin.getEntity();
            if (!entity || !entity->isType(ActorType::Player)) return;
            auto* player = static_cast<Player*>(entity);

            auto& session = WorldEditMod::getInstance().getSessionManager().getSelection(*player);
            if (!session.isComplete() || session.dimId.value() != player->getDimensionId()) {
                output.error("Invalid selection.");
                return;
            }

            auto result = params.blockName.resolveBlock(0);
            const Block* blk = result.mBlock;
            if (!blk) { output.error("Unknown block."); return; }

            std::vector<PatternEntry> pattern = {{blk, 1.0}};
            executeSetTask(player->getXuid(), session.pos1.value(), session.pos2.value(), pattern, session.dimId.value())
                .launch(ll::thread::ServerThreadExecutor::getDefault());
            output.success("Set operation started...");
        });

    setCmd.overload<SetParamsPattern>()
        .required("patternStr")
        .execute([](CommandOrigin const& origin, CommandOutput& output, SetParamsPattern const& params) {
            auto* entity = origin.getEntity();
            if (!entity || !entity->isType(ActorType::Player)) return;
            auto* player = static_cast<Player*>(entity);

            auto& session = WorldEditMod::getInstance().getSessionManager().getSelection(*player);
            if (!session.isComplete() || session.dimId.value() != player->getDimensionId()) {
                output.error("Invalid selection.");
                return;
            }

            auto pattern = parsePattern(params.patternStr.mText, output);
            if (pattern.empty()) return;

            executeSetTask(player->getXuid(), session.pos1.value(), session.pos2.value(), pattern, session.dimId.value())
                .launch(ll::thread::ServerThreadExecutor::getDefault());
            output.success("Pattern set operation started...");
        });
}

}
