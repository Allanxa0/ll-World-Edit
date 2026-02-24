#pragma once
#include "mc/world/level/BlockPos.h"
#include "mc/deps/core/utility/AutomaticID.h"
#include "mc/world/level/dimension/Dimension.h"
#include <optional>

namespace my_mod {

struct Selection {
    std::optional<BlockPos> pos1;
    std::optional<BlockPos> pos2;
    std::optional<DimensionType> dimId;

    bool isComplete() const {
        return pos1.has_value() && pos2.has_value() && dimId.has_value();
    }
};

}




