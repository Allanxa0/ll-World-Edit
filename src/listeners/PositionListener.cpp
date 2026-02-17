#include "PositionListener.h"
#include "WorldEditMod.h"
#include "ll/api/event/EventBus.h"
#include "mc/world/level/block/block_events/BlockBreakEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/world/actor/player/Player.h"

namespace my_mod {

void PositionListener::registerListeners() {
    auto& bus = ll::event::EventBus::getInstance();

    bus.addListener<ll::event::BlockBreakEvent>(
        [](ll::event::BlockBreakEvent& ev) {
            auto& player = ev.self();
            if (!player.isOperator()) return;

            auto& item = player.getSelectedItem();
            
            if (!item.isNull() && item.getItem()->getRawNameId() == "minecraft:wooden_axe") {
                WorldEditMod::getInstance().getSessionManager().setPos1(player, ev.getBlockPos());
                player.sendMessage("§dPosition 1 set to " + ev.getBlockPos().toString());
                ev.cancel();
            }
        }
    );

    bus.addListener<ll::event::player::PlayerInteractBlockEvent>(
        [](ll::event::player::PlayerInteractBlockEvent& ev) {
            auto& player = ev.self();
            if (!player.isOperator()) return;

            auto& item = player.getSelectedItem();
            
            if (!item.isNull() && item.getItem()->getRawNameId() == "minecraft:wooden_axe") {
                WorldEditMod::getInstance().getSessionManager().setPos2(player, ev.getBlockPos());
                player.sendMessage("§dPosition 2 set to " + ev.getBlockPos().toString());
                ev.cancel();
            }
        }
    );
}

}

