#include "WorldEditMod.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Command.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/phys/HitResult.h"
#include "mc/world/level/ShapeType.h"
#include "mc/world/level/dimension/Dimension.h"

namespace my_mod {

BlockPos getTargetBlock(Player* player) {
    const float maxDist = 20.0f;
    Vec3 pos = player->getEyePos();
    Vec3 dir = player->getViewVector(1.0f);
    Vec3 end = pos + (dir * maxDist);

    HitResult hit = player->getDimension().getBlockSourceFromMainChunkSource().clip(
        pos, 
        end, 
        false, 
        ShapeType::Outline, 
        static_cast<int>(maxDist), 
        false, 
        false, 
        player, 
        nullptr, 
        false
    );
    
    if (hit.mType == HitResultType::Tile) {
        return hit.mBlock;
    }
    return BlockPos(player->getPosition());
}

void registerPosCommands() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);

    auto& pos1Cmd = registrar.getOrCreateCommand("pos1", "Set position 1");
    pos1Cmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (!entity || !entity->isType(ActorType::Player)) {
            output.error("Only players can use this command");
            return;
        }

        auto* player = static_cast<Player*>(entity);
        BlockPos pos = getTargetBlock(player);
        
        WorldEditMod::getInstance().getSessionManager().setPos1(*player, pos);
        
        output.success("First position set to " + pos.toString());
    });

    auto& pos2Cmd = registrar.getOrCreateCommand("pos2", "Set position 2");
    pos2Cmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (!entity || !entity->isType(ActorType::Player)) {
            output.error("Only players can use this command");
            return;
        }

        auto* player = static_cast<Player*>(entity);
        BlockPos pos = getTargetBlock(player);

        WorldEditMod::getInstance().getSessionManager().setPos2(*player, pos);

        output.success("Second position set to " + pos.toString());
    });
}

}

