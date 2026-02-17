#include "PositionListener.h"
#include "WorldEditMod.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDestroyBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"

namespace my_mod {

void PositionListener::registerListeners() {
    auto& bus = ll::event::EventBus::getInstance();

    bus.addListener<ll::event::player::PlayerDestroyBlockEvent>(
        [](ll::event::player::PlayerDestroyBlockEvent& ev) {
            auto& player = ev.self();
            
            auto& item = player.getSelectedItem();
            if (!item.isNull() && item.getItem() && item.getItem()->getSerializedName() == "minecraft:wooden_axe") {
                
                // Según PlayerDestroyBlockEvent.cpp, el método es pos()
                BlockPos pos = ev.pos();
                
                WorldEditMod::getInstance().getSessionManager().setPos1(player, pos);
                
                player.sendMessage("§dPrimera posición establecida en " + pos.toString());
                
                ev.cancel();
            }
        }
    );

    bus.addListener<ll::event::player::PlayerInteractBlockEvent>(
        [](ll::event::player::PlayerInteractBlockEvent& ev) {
            auto& player = ev.self();

            auto& item = player.getSelectedItem();
            if (!item.isNull() && item.getItem() && item.getItem()->getSerializedName() == "minecraft:wooden_axe") {
                
                // Según PlayerInteractBlockEvent.h, el método es blockPos()
                BlockPos pos = ev.blockPos();
                
                WorldEditMod::getInstance().getSessionManager().setPos2(player, pos);
                
                player.sendMessage("§dSegunda posición establecida en " + pos.toString());
                
                ev.cancel();
            }
        }
    );
}

}
