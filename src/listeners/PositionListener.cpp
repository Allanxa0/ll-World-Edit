#include "PositionListener.h"
#include "WorldEditMod.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/player/PlayerDestroyBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"

namespace my_mod {

void PositionListener::registerListeners() {
    auto& bus = ll::event::EventBus::getInstance();

    auto destroyListener = ll::event::Listener<ll::event::player::PlayerDestroyBlockEvent>::create([](ll::event::player::PlayerDestroyBlockEvent& ev) {
        auto& player = ev.self();
        auto& item = player.getSelectedItem();
        
        if (!item.isNull() && item.getItem() && item.getItem()->getSerializedName() == "minecraft:wooden_axe") {
            if (!WorldEditMod::getInstance().getSessionManager().canUseWand(player)) {
                ev.cancel(); 
                return;
            }

            BlockPos pos = ev.pos();
            WorldEditMod::getInstance().getSessionManager().setPos1(player, pos);
            WorldEditMod::getInstance().getSessionManager().updateWandUsage(player);
            
            player.sendMessage("§dPrimera posición establecida en " + pos.toString());
            ev.cancel();
        }
    });
    bus.addListener<ll::event::player::PlayerDestroyBlockEvent>(destroyListener);

    auto interactListener = ll::event::Listener<ll::event::player::PlayerInteractBlockEvent>::create([](ll::event::player::PlayerInteractBlockEvent& ev) {
        auto& player = ev.self();
        auto& item = player.getSelectedItem();

        if (!item.isNull() && item.getItem() && item.getItem()->getSerializedName() == "minecraft:wooden_axe") {
            if (!WorldEditMod::getInstance().getSessionManager().canUseWand(player)) {
                ev.cancel();
                return;
            }

            BlockPos pos = ev.blockPos();
            WorldEditMod::getInstance().getSessionManager().setPos2(player, pos);
            WorldEditMod::getInstance().getSessionManager().updateWandUsage(player);

            player.sendMessage("§dSegunda posición establecida en " + pos.toString());
            ev.cancel();
        }
    });
    bus.addListener<ll::event::player::PlayerInteractBlockEvent>(interactListener);
}

}

