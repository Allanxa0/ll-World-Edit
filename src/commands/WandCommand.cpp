#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Command.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/nbt/CompoundTag.h"
#include "mc/nbt/ListTag.h"
#include <memory>

namespace my_mod {

void registerWandCommand() {
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);

    auto& wandCmd = registrar.getOrCreateCommand("wand", "Get the WorldEdit wand");
    wandCmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (!entity || !entity->isType(ActorType::Player)) {
            output.error("Only players can use this command");
            return;
        }

        auto* player = static_cast<Player*>(entity);
        
        ItemStack item;
        item.reinit("minecraft:wooden_axe", 1, 0);
        
        auto tag = std::make_unique<CompoundTag>();
        (*tag)["ench"] = ListTag();
        item.setUserData(std::move(tag));
        
        player->addAndRefresh(item);

        output.success("Left click: select pos #1; Right click: select pos #2");
    });
}

}




