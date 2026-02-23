#include "WorldEditMod.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Command.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/nbt/CompoundTag.h"
#include "mc/nbt/ListTag.h"
#include "mc/nbt/IntTag.h"
#include "mc/nbt/ShortTag.h"
#include "mc/nbt/ByteArrayTag.h"
#include "mc/nbt/StringTag.h"
#include "mc/world/level/block/Block.h"
#include "mc/deps/core/string/HashedString.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace my_mod {

struct SchematicParams {
    std::string filename;
};

void createSchematicDir() {
    auto dir = WorldEditMod::getInstance().getSelf().getConfigDir() / "schematics";
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }
}

void registerSchematicCommand() {
    createSchematicDir();
    auto& registrar = ll::command::CommandRegistrar::getInstance(false);
    
    auto& schemCmd = registrar.getOrCreateCommand("schematic");
    schemCmd.alias("schem");

    schemCmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* entity = origin.getEntity();
        if (entity && entity->isType(ActorType::Player)) {
            if (!static_cast<Player*>(entity)->isOperator()) {
                output.error("Solo los operadores pueden usar este comando.");
                return;
            }
        }
        
        auto dir = WorldEditMod::getInstance().getSelf().getConfigDir() / "schematics";
        std::string list = "§aSchematics disponibles:\n";
        bool found = false;
        if (std::filesystem::exists(dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().extension() == ".schematic" || entry.path().extension() == ".schem" || entry.path().extension() == ".mcstructure") {
                    list += "§7- " + entry.path().filename().string() + "\n";
                    found = true;
                }
            }
        }
        if (!found) list += "§cNo se encontraron schematics.";
        output.success(list);
    });

    schemCmd.overload<SchematicParams>()
        .required("filename")
        .execute([](CommandOrigin const& origin, CommandOutput& output, SchematicParams const& params) {
            auto* entity = origin.getEntity();
            if (!entity || !entity->isType(ActorType::Player)) return;
            auto* player = static_cast<Player*>(entity);

            if (!player->isOperator()) {
                output.error("Solo los operadores pueden usar este comando.");
                return;
            }

            auto dir = WorldEditMod::getInstance().getSelf().getConfigDir() / "schematics";
            std::filesystem::path filepath = dir / params.filename;

            if (!std::filesystem::exists(filepath)) {
                if (std::filesystem::exists(dir / (params.filename + ".schem"))) {
                    filepath = dir / (params.filename + ".schem");
                } else if (std::filesystem::exists(dir / (params.filename + ".schematic"))) {
                    filepath = dir / (params.filename + ".schematic");
                } else if (std::filesystem::exists(dir / (params.filename + ".mcstructure"))) {
                    filepath = dir / (params.filename + ".mcstructure");
                } else {
                    output.error("Schematic not found.");
                    return;
                }
            }

            std::ifstream file(filepath, std::ios::binary);
            if (!file) {
                output.error("Failed to open schematic.");
                return;
            }

            std::ostringstream ss;
            ss << file.rdbuf();
            
            auto nbtResult = CompoundTag::fromBinaryNbt(ss.str(), true);
            if (!nbtResult) {
                output.error("Failed to parse NBT data.");
                return;
            }

            int count = 0;
            std::vector<ClipboardItem> clipboard;
            auto airOpt = Block::tryGetFromRegistry(HashedString("minecraft:air"));
            const Block* defaultBlock = airOpt ? &(*airOpt) : nullptr;

            if (nbtResult->contains("size")) {
                auto const& sizeTag = (*nbtResult)["size"];
                if (sizeTag.is_array() && sizeTag.size() >= 3) {
                    int width = (int)sizeTag[0];
                    int height = (int)sizeTag[1];
                    int length = (int)sizeTag[2];
                    count = width * height * length;
                    clipboard.reserve(count);

                    std::vector<const Block*> paletteBlocks;
                    if (nbtResult->contains("structure") && (*nbtResult)["structure"].contains("palette")) {
                        auto const& paletteTag = (*nbtResult)["structure"]["palette"]["default"]["block_palette"];
                        if (paletteTag.is_array()) {
                            for (size_t i = 0; i < paletteTag.size(); ++i) {
                                auto blockOpt = Block::tryGetFromRegistry(paletteTag[i].get<CompoundTag>());
                                paletteBlocks.push_back(blockOpt ? &(*blockOpt) : defaultBlock);
                            }
                        }
                    }

                    if (paletteBlocks.empty()) paletteBlocks.push_back(defaultBlock);

                    std::vector<int> blockIndices;
                    if (nbtResult->contains("structure") && (*nbtResult)["structure"].contains("block_indices")) {
                        auto const& indicesList = (*nbtResult)["structure"]["block_indices"];
                        if (indicesList.is_array() && indicesList.size() > 0) {
                            auto const& layer0 = indicesList[0];
                            if (layer0.getId() == Tag::Type::ByteArray) {
                                auto const& arr = layer0.get<ByteArrayTag>().mData;
                                for (auto b : arr) blockIndices.push_back((int)b);
                            } else if (layer0.getId() == Tag::Type::IntArray) {
                                auto const& arr = layer0.get<IntArrayTag>().mData;
                                for (auto b : arr) blockIndices.push_back(b);
                            } else if (layer0.is_array()) {
                                for (size_t i = 0; i < layer0.size(); ++i) {
                                    blockIndices.push_back((int)layer0[i]);
                                }
                            }
                        }
                    }

                    int idx = 0;
                    for (int x = 0; x < width; ++x) {
                        for (int y = 0; y < height; ++y) {
                            for (int z = 0; z < length; ++z) {
                                const Block* b = defaultBlock;
                                if (idx < blockIndices.size()) {
                                    int pIdx = blockIndices[idx];
                                    if (pIdx >= 0 && pIdx < paletteBlocks.size()) {
                                        b = paletteBlocks[pIdx];
                                    }
                                }
                                clipboard.push_back({BlockPos(x, y, z), b, nullptr});
                                idx++;
                            }
                        }
                    }
                }
            } else if (nbtResult->contains("Width")) {
                int width = (short)(*nbtResult)["Width"];
                int height = (short)(*nbtResult)["Height"];
                int length = (short)(*nbtResult)["Length"];
                count = width * height * length;
                clipboard.reserve(count);

                std::vector<const Block*> paletteBlocks(256, defaultBlock);
                if (nbtResult->contains("Materials") && (std::string)(*nbtResult)["Materials"] == "Alpha") {
                    if (nbtResult->contains("Blocks") && nbtResult->contains("Data")) {
                        auto const& blocksData = (*nbtResult)["Blocks"].get<ByteArrayTag>().mData;
                        auto const& dataData = (*nbtResult)["Data"].get<ByteArrayTag>().mData;
                        
                        int idx = 0;
                        for (int y = 0; y < height; ++y) {
                            for (int z = 0; z < length; ++z) {
                                for (int x = 0; x < width; ++x) {
                                    if (idx < blocksData.size() && idx < dataData.size()) {
                                        uint8_t bId = blocksData[idx];
                                        uint8_t dId = dataData[idx];
                                        auto blockOpt = Block::tryGetFromRegistry((uint)bId, (ushort)dId);
                                        const Block* b = blockOpt ? &(*blockOpt) : defaultBlock;
                                        clipboard.push_back({BlockPos(x, y, z), b, nullptr});
                                    } else {
                                        clipboard.push_back({BlockPos(x, y, z), defaultBlock, nullptr});
                                    }
                                    idx++;
                                }
                            }
                        }
                    }
                } else if (nbtResult->contains("Palette")) {
                    std::map<int, const Block*> paletteMap;
                    auto const& paletteTag = (*nbtResult)["Palette"].get<CompoundTag>();
                    for (auto const& [name, val] : paletteTag) {
                        int id = (int)val;
                        auto blockOpt = Block::tryGetFromRegistry(HashedString(name));
                        paletteMap[id] = blockOpt ? &(*blockOpt) : defaultBlock;
                    }

                    if (nbtResult->contains("BlockData")) {
                        auto const& blocksData = (*nbtResult)["BlockData"].get<ByteArrayTag>().mData;
                        int idx = 0;
                        for (int y = 0; y < height; ++y) {
                            for (int z = 0; z < length; ++z) {
                                for (int x = 0; x < width; ++x) {
                                    const Block* b = defaultBlock;
                                    if (idx < blocksData.size()) {
                                        int pIdx = blocksData[idx];
                                        if (paletteMap.count(pIdx)) b = paletteMap[pIdx];
                                    }
                                    clipboard.push_back({BlockPos(x, y, z), b, nullptr});
                                    idx++;
                                }
                            }
                        }
                    }
                } else {
                    for (int x = 0; x < width; ++x) {
                        for (int y = 0; y < height; ++y) {
                            for (int z = 0; z < length; ++z) {
                                clipboard.push_back({BlockPos(x, y, z), defaultBlock, nullptr});
                            }
                        }
                    }
                }
            } else {
                output.error("Failed to parse NBT data.");
                return;
            }

            WorldEditMod::getInstance().getSessionManager().setClipboard(*player, std::move(clipboard));
            player->sendMessage("§aSe han copiado " + std::to_string(count) + " bloques del schematic.");
        });
}

}