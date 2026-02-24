#include "PositionListener.h"
#include "WorldEditMod.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/player/PlayerDestroyBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/Item.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"
#include <chrono>

using namespace std::chrono_literals;

namespace my_mod {

ll::coro::CoroTask<void> visualUpdateTask() {
    while (true) {
        co_await 3s;
        auto levelOpt = ll::service::getLevel();
        if (!levelOpt.has_value()) continue;
        
        levelOpt->forEachPlayer([](Player& player) -> bool {
            if (player.isOperator()) {
                auto& sessionManager = WorldEditMod::getInstance().getSessionManager();
                auto& sel = sessionManager.getSelection(player);
                if (sel.isComplete() && sel.dimId.has_value() && sel.dimId.value() == player.getDimensionId()) {
                    sessionManager.updateSelectionVisuals(player);
                }
            }
            return true;
        });
    }
}

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

    auto joinListener = ll::event::Listener<ll::event::player::PlayerJoinEvent>::create([](ll::event::player::PlayerJoinEvent& ev) {
        WorldEditMod::getInstance().getSessionManager().updateSelectionVisuals(ev.self());
    });
    bus.addListener<ll::event::player::PlayerJoinEvent>(joinListener);

    visualUpdateTask().launch(ll::thread::ServerThreadExecutor::getDefault());
}

}
