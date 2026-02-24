#pragma once

namespace my_mod {

struct Config {
    int wandCooldownMs = 400;
    int maxHistorySize = 10;
    int maxBlocksPerOperation = 500000;
    int blocksPerTick = 3000;
    int chunksPerTick = 3;
    
    int version = 1;
};

}




