#pragma once

#include "ll/api/mod/NativeMod.h"
#include "core/SessionManager.h"

namespace my_mod {

class WorldEditMod {

public:
    static WorldEditMod& getInstance();

    WorldEditMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    [[nodiscard]] SessionManager& getSessionManager() { return mSessionManager; }

    bool load();
    bool enable();
    bool disable();

private:
    ll::mod::NativeMod& mSelf;
    SessionManager mSessionManager;
};

}
