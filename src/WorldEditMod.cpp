#include "WorldEditMod.h"
#include "commands/WECommands.h"
#include "listeners/PositionListener.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"

namespace my_mod {

WorldEditMod& WorldEditMod::getInstance() {
    static WorldEditMod instance;
    return instance;
}

bool WorldEditMod::load() {
    return true;
}

bool WorldEditMod::enable() {
    WECommands::registerCommands();
    PositionListener::registerListeners();

    auto& bus = ll::event::EventBus::getInstance();

    auto listener = ll::event::Listener<ll::event::player::PlayerDisconnectEvent>::create(
        [this](ll::event::player::PlayerDisconnectEvent& ev) {
            mSessionManager.removeSelection(ev.self());
        }
    );
    bus.addListener(listener);

    return true;
}

bool WorldEditMod::disable() {
    return true;
}

}

LL_REGISTER_MOD(my_mod::WorldEditMod, my_mod::WorldEditMod::getInstance());
