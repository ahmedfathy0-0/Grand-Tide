#include "survival-system.hpp"
#include "../asset-loader.hpp"

namespace our {

    namespace {
        Entity* getWeaponEntity(World* world, Entity* playerEntity) {
            if(!world || !playerEntity) return nullptr;
            for(auto entity : world->getEntities()) {
                if(entity->parent == playerEntity && entity->name == "weapon") {
                    return entity;
                }
            }
            return nullptr;
        }

        MeshRendererComponent* getWeaponRenderer(World* world, Entity* playerEntity) {
            auto* weaponEntity = getWeaponEntity(world, playerEntity);
            if(!weaponEntity) return nullptr;
            return weaponEntity->getComponent<MeshRendererComponent>();
        }

        void setWeaponMesh(
            World* world,
            Entity* playerEntity,
            const std::string& meshName,
            const glm::vec3& scale,
            const glm::vec3& rotationDegrees
        ) {
            auto meshR = getWeaponRenderer(world, playerEntity);
            auto* weaponEntity = getWeaponEntity(world, playerEntity);
            if(!meshR || !weaponEntity) return;

            if(auto* mesh = AssetLoader<Mesh>::get(meshName)) {
                meshR->mesh = mesh;
            }

            // Keep visual size consistent across imported tool models.
            weaponEntity->localTransform.scale = scale;
            weaponEntity->localTransform.rotation = glm::radians(rotationDegrees);
        }
    }

    void SurvivalSystem::handleBoatDamaged(int damage) {
        if(!boatEntity) return;

        auto boatHealth = boatEntity->getComponent<HealthComponent>();
        if(!boatHealth) return;

        boatHealth->takeDamage(static_cast<float>(damage));
        std::cout << "[Event] Boat damaged for " << damage << " hp. Current: " << boatHealth->currentHealth << "\n";
        if(boatHealth->isDead()) {
            std::cout << "[Event] Boat destroyed!\n";
            auto meshR = boatEntity->getComponent<MeshRendererComponent>();
            if(meshR) {
                meshR->material = AssetLoader<Material>::get("dark_broken");
            }
        }
    }

    void SurvivalSystem::handleRepairAction() {
        if(!playerEntity || !boatEntity) return;

        auto inventory = playerEntity->getComponent<InventoryComponent>();
        auto boatHealth = boatEntity->getComponent<HealthComponent>();
        if(!inventory || !boatHealth) return;

        if(inventory->activeSlot != 1 || inventory->woodCount <= 0) return;
        if(boatHealth->currentHealth >= boatHealth->maxHealth) return;

        const glm::vec3 playerPos = glm::vec3(playerEntity->getLocalToWorldMatrix()[3]);
        const glm::vec3 boatPos = glm::vec3(boatEntity->getLocalToWorldMatrix()[3]);
        const float dist = glm::distance(playerPos, boatPos);
        if(dist > 4.5f) return;

        boatHealth->heal(100.0f);
        inventory->woodCount--;
        std::cout << "[Repair] Boat repaired! Wood left: " << inventory->woodCount << "\n";

        auto meshR = boatEntity->getComponent<MeshRendererComponent>();
        if(meshR) {
            meshR->material = AssetLoader<Material>::get("raft_mat");
        }
    }

    void SurvivalSystem::handleGatherAction() {
        if(!world || !playerEntity) return;

        auto inventory = playerEntity->getComponent<InventoryComponent>();
        if(!inventory || inventory->activeSlot != 2) return;

        const glm::vec3 playerPos = glm::vec3(playerEntity->getLocalToWorldMatrix()[3]);

        for(auto entity : world->getEntities()) {
            auto res = entity->getComponent<ResourceComponent>();
            if(!res) continue;

            const glm::vec3 resPos = glm::vec3(entity->getLocalToWorldMatrix()[3]);
            const float dist = glm::distance(playerPos, resPos);
            if(dist >= 3.0f) continue;

            if(res->type == "wood") {
                inventory->woodCount += res->amount;
                std::cout << "[Gather] Gathered wood\n";
            } else if(res->type == "fish") {
                inventory->fishCount += res->amount;
                std::cout << "[Gather] Gathered fish\n";
            }
            world->markForRemoval(entity);
        }
    }

    void SurvivalSystem::handleAttackAction() {
        if(!playerEntity) return;
        auto inventory = playerEntity->getComponent<InventoryComponent>();
        if(!inventory || inventory->activeSlot != 3) return;

        std::cout << "[Combat] Attacked with Spear!\n";
        EventManager::emit("PLAYER_ATTACKED", 25);
    }

    void SurvivalSystem::update() {
        if (!playerEntity || !boatEntity) return;

        auto inventory = playerEntity->getComponent<InventoryComponent>();
        auto playerHealth = playerEntity->getComponent<HealthComponent>();
        if(!inventory || !playerHealth) return;

        auto& keyboard = app->getKeyboard();
        auto& mouse = app->getMouse();

        // 1. Tool Switching
        if (keyboard.justPressed(GLFW_KEY_1)) {
            inventory->activeSlot = 1;
            std::cout << "[Tool] Switched to Hammer\n";
            setWeaponMesh(world, playerEntity, "hammer", glm::vec3(0.1f), glm::vec3(45.0f, 45.0f, 0.0f));
        }
        if (keyboard.justPressed(GLFW_KEY_2)) {
            inventory->activeSlot = 2;
            std::cout << "[Tool] Switched to Net\n";
            setWeaponMesh(world, playerEntity, "net", glm::vec3(0.004f), glm::vec3(45.0f, 225.0f, 0.0f));
        }
        if (keyboard.justPressed(GLFW_KEY_3)) {
            inventory->activeSlot = 3;
            std::cout << "[Tool] Switched to Spear\n";
            setWeaponMesh(world, playerEntity, "spear", glm::vec3(0.018f), glm::vec3(45.0f, 45.0f, 90.0f));
        }

        // 2. Consuming
        if (keyboard.justPressed(GLFW_KEY_E)) {
            if (inventory->fishCount > 0 && playerHealth->currentHealth < playerHealth->maxHealth) {
                playerHealth->heal(20.0f);
                inventory->fishCount--;
                std::cout << "[Consume] Ate fish. Health now: " << playerHealth->currentHealth << "\n";
            }
        }

        // 3. Debug Damage to Boat
        if (keyboard.justPressed(GLFW_KEY_K)) {
            EventManager::emit("BOAT_DAMAGED", 100);
        }

        // 4. Tool Actions (event-driven requests)
        if(mouse.justPressed(GLFW_MOUSE_BUTTON_1)) {
            if(inventory->activeSlot == 1) {
                EventManager::emit("PLAYER_REPAIR_ACTION");
            } else if(inventory->activeSlot == 2) {
                EventManager::emit("PLAYER_GATHER_ACTION");
            } else if(inventory->activeSlot == 3) {
                EventManager::emit("PLAYER_ATTACK_ACTION");
            }
        }
    }
}
