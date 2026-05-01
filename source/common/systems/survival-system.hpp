#pragma once

#include "../ecs/world.hpp"
#include "../ecs/event-manager.hpp"
#include "../components/health.hpp"
#include "../components/inventory.hpp"
#include "../components/resource.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/shark-component.hpp"
#include "../application.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <iostream>

namespace our {

    struct ToolTransform {
        glm::vec3 localPosition; // offset from player in player's local space
        glm::vec3 localRotation; // rotation offset in radians
        glm::vec3 scale;         // world scale (independent of parent)
    };

    class SurvivalSystem {
        World* world;
        Application* app;
        Entity* playerEntity;
        Entity* boatEntity;
        Entity* weaponEntity = nullptr;
        bool eventsSubscribed = false;

        // Per-tool transform configs
        ToolTransform hammerTransform;
        ToolTransform netTransform;
        ToolTransform spearTransform;
        ToolTransform* activeToolTransform = nullptr;

        void handleBoatDamaged(int damage);
        void handleRepairAction();
        void handleGatherAction();
        void handleAttackAction();

    public:
        void setup(World* world, Application* app, Entity* player, Entity* boat) {
            this->world = world;
            this->app = app;
            this->playerEntity = player;
            this->boatEntity = boat;

            // Find weapon entity (now a world-level entity, not child of player)
            for (auto entity : world->getEntities()) {
                if (entity->name == "weapon") {
                    weaponEntity = entity;
                    break;
                }
            }

            // Read initial scale and rotation from the weapon entity (set via app.jsonc)
            glm::vec3 jsonScale = weaponEntity ? weaponEntity->localTransform.scale : glm::vec3(0.1f);
            glm::vec3 jsonRotation = weaponEntity ? weaponEntity->localTransform.rotation : glm::radians(glm::vec3(120.0f, 90.0f, 90.0f));

            // Per-tool transforms: position offset (player local space),
            // rotation offset (radians from JSON), and scale (from JSON)
            hammerTransform = {
                glm::vec3(1.0f, -1.0f, -5.0f),
                jsonRotation,
                jsonScale
            };
            netTransform = {
                glm::vec3(1.0f, -1.0f, -5.0f),
                jsonRotation,
                jsonScale
            };
            spearTransform = {
                glm::vec3(1.0f, -1.0f, -5.0f),
                jsonRotation,
                jsonScale
            };
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
    };

}