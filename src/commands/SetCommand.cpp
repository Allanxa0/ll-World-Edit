#include "WorldEditMod.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Command.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/registry/BlockTypeRegistry.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/IDimension.h"
#include <algorithm>

namespace my_mod {

struct SetParams {
    std::string pattern;
};

void registerSetCommand() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);

    auto& setCmd = registrar.getOrCreateCommand("set", "Set blocks in selection");
    setCmd.overload<SetParams>()
        .required("pattern")
        .execute([](CommandOrigin const& origin, CommandOutput& output, SetParams const& params) {
            auto* entity = origin.getEntity();
            if (!entity || !entity->isType(ActorType::Player)) {
                output.error("Only players can use this command");
                return;
            }

            auto* player = static_cast<Player*>(entity);
            auto& session = WorldEditMod::getInstance().getSessionManager().getSelection(*player);

            if (!session.isComplete()) {
                output.error("Make a selection first.");
                return;
            }

            auto blockOpt = Block::tryGetFromRegistry(params.pattern);
            if (!blockOpt.has_value()) {
                output.error("Invalid block pattern: " + params.pattern);
                return;
            }
            const Block& blockToSet = *blockOpt;

            BlockPos p1 = session.pos1.value();
            BlockPos p2 = session.pos2.value();

            int minX = std::min(p1.x, p2.x);
            int minY = std::min(p1.y, p2.y);
            int minZ = std::min(p1.z, p2.z);
            int maxX = std::max(p1.x, p2.x);
            int maxY = std::max(p1.y, p2.y);
            int maxZ = std::max(p1.z, p2.z);

            auto& region = player->getDimension().getBlockSourceFromMainChunkSource();
            int count = 0;

            for (int x = minX; x <= maxX; ++x) {
                for (int y = minY; y <= maxY; ++y) {
                    for (int z = minZ; z <= maxZ; ++z) {
                        region.setBlock({x, y, z}, blockToSet, 3);
                        count++;
                    }
                }
            }

            output.success("Operation completed. " + std::to_string(count) + " blocks affected.");
        });
}

}
