#include "WorldEditMod.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/Command.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace my_mod {

struct SchematicParams {
    std::string action;
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
    
    auto& schemCmd = registrar.getOrCreateCommand("schem", "Manage schematics");
    schemCmd.alias("schematics");

    schemCmd.overload().execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto dir = WorldEditMod::getInstance().getSelf().getConfigDir() / "schematics";
        std::string list = "§aSchematics disponibles:\n";
        bool found = false;
        if (std::filesystem::exists(dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().extension() == ".schematic" || entry.path().extension() == ".schem") {
                    list += "§7- " + entry.path().filename().string() + "\n";
                    found = true;
                }
            }
        }
        if (!found) list += "§cNo se encontraron schematics.";
        output.success(list);
    });

    schemCmd.overload<SchematicParams>()
        .required("action")
        .required("filename")
        .execute([](CommandOrigin const& origin, CommandOutput& output, SchematicParams const& params) {
            auto* entity = origin.getEntity();
            if (!entity || !entity->isType(ActorType::Player)) return;
            auto* player = static_cast<Player*>(entity);

            if (params.action != "load") {
                output.error("Action must be 'load'");
                return;
            }

            auto filepath = WorldEditMod::getInstance().getSelf().getConfigDir() / "schematics" / params.filename;
            if (!std::filesystem::exists(filepath)) {
                output.error("Schematic not found.");
                return;
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

            output.success("§aSchematic guardado en el NBT, decodificación completa pendiente. (Estructura cargada en memoria).");
        });
}

}
