#pragma once

#include "ll/api/mod/NativeMod.h"
#include "core/SessionManager.h"
#include "core/Config.h"

namespace my_mod {

class WorldEditMod {
public:
    static WorldEditMod& getInstance();

    WorldEditMod() = default;

    [[nodiscard]] ll::mod::NativeMod& getSelf() const {
        return *ll::mod::NativeMod::current();
    }

    [[nodiscard]] SessionManager& getSessionManager() {
        return mSessionManager;
    }

    [[nodiscard]] Config& getConfig() {
        return mConfig;
    }

    bool load();
    bool enable();
    bool disable();

private:
    SessionManager mSessionManager;
    Config mConfig;
};

}



