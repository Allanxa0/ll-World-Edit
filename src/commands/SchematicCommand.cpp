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
#include "mc/world/level/block/Block.h"
#include <filesystem>
#include <fstream>
#include <sstream>

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

    schemCmd.overload().execute([](CommandOrigin const&, CommandOutput& output) {
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
        .execute([](CommandOrigin const&, CommandOutput& output, SchematicParams const& params) {
            auto* entity = CommandOrigin::getEntity();
            if (!entity || !entity->isType(ActorType::Player)) return;
            auto* player = static_cast<Player*>(entity);

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

            if (nbtResult->contains("size")) {
                auto const& sizeTag = (*nbtResult)["size"];
                if (sizeTag.isListTag()) {
                    auto const& sizeList = sizeTag.asListTag();
                    if (sizeList.size() >= 3) {
                        int width = sizeList.get(0)->as<IntTag>().mValue;
                        int height = sizeList.get(1)->as<IntTag>().mValue;
                        int length = sizeList.get(2)->as<IntTag>().mValue;
                        count = width * height * length;

                        auto airOpt = Block::tryGetFromRegistry(std::string_view("minecraft:air"));
                        const Block* defaultBlock = airOpt ? &(*airOpt) : nullptr;
                        
                        clipboard.reserve(count);
                        for (int x = 0; x < width; ++x) {
                            for (int y = 0; y < height; ++y) {
                                for (int z = 0; z < length; ++z) {
                                    clipboard.push_back({BlockPos(x, y, z), defaultBlock, nullptr});
                                }
                            }
                        }
                    }
                }
            } else if (nbtResult->contains("Width")) {
                int width = (*nbtResult)["Width"].as<ShortTag>().mValue;
                int height = (*nbtResult)["Height"].as<ShortTag>().mValue;
                int length = (*nbtResult)["Length"].as<ShortTag>().mValue;
                count = width * height * length;

                auto airOpt = Block::tryGetFromRegistry(std::string_view("minecraft:air"));
                const Block* defaultBlock = airOpt ? &(*airOpt) : nullptr;

                clipboard.reserve(count);
                for (int x = 0; x < width; ++x) {
                    for (int y = 0; y < height; ++y) {
                        for (int z = 0; z < length; ++z) {
                            clipboard.push_back({BlockPos(x, y, z), defaultBlock, nullptr});
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
