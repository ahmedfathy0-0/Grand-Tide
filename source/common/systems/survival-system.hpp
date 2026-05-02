#pragma once

#include "../ecs/world.hpp"
#include "../ecs/event-manager.hpp"
#include "../components/health.hpp"
#include "../components/inventory.hpp"
#include "../components/resource.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/shark-component.hpp"
#include "../application.hpp"
#include "audio-player.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include "../deserialize-utils.hpp"
#include <iostream>

namespace our {

    struct ToolTransform {
        glm::vec3 localPosition; // offset from player in player's local space (right, up, forward)
        glm::vec3 localRotation; // rotation in radians, added on top of player rotation
        glm::vec3 scale;         // world scale applied directly
    };

    class SurvivalSystem {
        World* world;
        Application* app;
        Entity* playerEntity;
        Entity* boatEntity;
        Entity* weaponEntity = nullptr;
        bool eventsSubscribed = false;

        // Per-tool transform configs (loaded from app.jsonc at setup)
        ToolTransform hammerTransform;
        ToolTransform netTransform;
        ToolTransform spearTransform;
        ToolTransform* activeToolTransform = nullptr;

        // Pain sound player
        AudioPlayer painSoundPlayer;

        void handleBoatDamaged(int damage);
        void handleRepairAction();
        void handleGatherAction();
        void handleAttackAction();

        // Parse a single tool block from the JSON ToolConfig component
        static ToolTransform parseToolTransform(const nlohmann::json& toolJson) {
            ToolTransform t;
            t.localPosition = toolJson.contains("position") ? toolJson["position"].get<glm::vec3>() : glm::vec3(1.0f, -1.0f, -5.0f);
            t.localRotation = glm::radians(toolJson.contains("rotation") ? toolJson["rotation"].get<glm::vec3>() : glm::vec3(0.0f));
            t.scale         = toolJson.contains("scale")    ? toolJson["scale"].get<glm::vec3>()    : glm::vec3(0.1f);
            return t;
        }

    public:
        void setup(World* world, Application* app, Entity* player, Entity* boat) {
            this->world = world;
            this->app = app;
            this->playerEntity = player;
            this->boatEntity = boat;

            // Find weapon entity (world-level entity, not child of player)
            for (auto entity : world->getEntities()) {
                if (entity->name == "weapon") {
                    weaponEntity = entity;
                    break;
                }
            }

            // Default fallback transforms
            ToolTransform fallback = {
                glm::vec3(1.0f, -1.0f, -5.0f),
                glm::radians(glm::vec3(0.0f)),
                glm::vec3(0.1f)
            };
            hammerTransform = netTransform = spearTransform = fallback;

            // Read per-tool configs from app.jsonc:
            //   scene > world > (entity "weapon") > components > (type "ToolConfig")
            const auto& config = app->getConfig();
            if (config.contains("scene") && config["scene"].contains("world")) {
                for (const auto& entityJson : config["scene"]["world"]) {
                    if (entityJson.value("name", "") != "weapon") continue;
                    if (!entityJson.contains("components")) break;
                    for (const auto& comp : entityJson["components"]) {
                        if (comp.value("type", "") != "ToolConfig") continue;
                        if (comp.contains("hammer")) hammerTransform = parseToolTransform(comp["hammer"]);
                        if (comp.contains("net"))    netTransform    = parseToolTransform(comp["net"]);
                        if (comp.contains("spear"))  spearTransform  = parseToolTransform(comp["spear"]);
                        break;
                    }
                    break;
                }
            }
            activeToolTransform = &hammerTransform;

            if(!eventsSubscribed) {
                EventManager::subscribe("BOAT_DAMAGED", [this](int damage) { handleBoatDamaged(damage); });
                EventManager::subscribe("PLAYER_REPAIR_ACTION", [this](int) { handleRepairAction(); });
                EventManager::subscribe("PLAYER_GATHER_ACTION", [this](int) { handleGatherAction(); });
                EventManager::subscribe("PLAYER_ATTACK_ACTION", [this](int) { handleAttackAction(); });
                eventsSubscribed = true;
            }
        }

        Entity* getWeaponEntity() const { return weaponEntity; }

        void update();

    private:
        void playPainSound();
    };

}