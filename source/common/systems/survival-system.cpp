#include "survival-system.hpp"
#include "../asset-loader.hpp"

namespace our {

    void SurvivalSystem::update() {
        if (!playerEntity || !boatEntity) return;

        auto inventory = playerEntity->getComponent<InventoryComponent>();
        auto playerHealth = playerEntity->getComponent<HealthComponent>();
        auto boatHealth = boatEntity->getComponent<HealthComponent>();

        auto& keyboard = app->getKeyboard();
        auto& mouse = app->getMouse();

        auto getWeaponR = [this]() -> MeshRendererComponent* {
            for(auto entity : world->getEntities()) {
                if(entity->parent == playerEntity && entity->name == "weapon") {
                    return entity->getComponent<MeshRendererComponent>();
                }
            }
            return nullptr;
        };

        // 1. Tool Switching
        if (keyboard.justPressed(GLFW_KEY_1)) {
            inventory->activeSlot = 1;
            std::cout << "[Tool] Switched to Hammer\n";
            if (auto meshR = getWeaponR()) meshR->material = AssetLoader<Material>::get("brown");
        }
        if (keyboard.justPressed(GLFW_KEY_2)) {
            inventory->activeSlot = 2;
            std::cout << "[Tool] Switched to Net\n";
            if (auto meshR = getWeaponR()) meshR->material = AssetLoader<Material>::get("green");
        }
        if (keyboard.justPressed(GLFW_KEY_3)) {
            inventory->activeSlot = 3;
            std::cout << "[Tool] Switched to Spear\n";
            if (auto meshR = getWeaponR()) meshR->material = AssetLoader<Material>::get("silver");
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

        // 4. Tool Actions
        if (mouse.justPressed(GLFW_MOUSE_BUTTON_1)) {
            glm::vec3 playerPos = glm::vec3(playerEntity->getLocalToWorldMatrix()[3]);

            if (inventory->activeSlot == 1) { // Hammer
                glm::vec3 boatPos = glm::vec3(boatEntity->getLocalToWorldMatrix()[3]);
                float dist = glm::distance(playerPos, boatPos);
                
                if (dist < 4.0f && boatHealth->currentHealth < boatHealth->maxHealth && inventory->woodCount > 0) {
                    boatHealth->heal(100.0f);
                    inventory->woodCount--;
                    std::cout << "[Repair] Boat repaired! Wood left: " << inventory->woodCount << "\n";
                    
                    // Swap back to original material (Red Glass)
                    auto meshR = boatEntity->getComponent<MeshRendererComponent>();
                    if (meshR) {
                        meshR->material = AssetLoader<Material>::get("red_glass");
                    }
                }
            } 
            else if (inventory->activeSlot == 2) { // Net
                for (auto entity : world->getEntities()) {
                    auto res = entity->getComponent<ResourceComponent>();
                    if (res) {
                        glm::vec3 resPos = glm::vec3(entity->getLocalToWorldMatrix()[3]);
                        float dist = glm::distance(playerPos, resPos);
                        if (dist < 3.0f) {
                            if (res->type == "wood") {
                                inventory->woodCount += res->amount;
                                std::cout << "[Gather] Gathered wood\n";
                            } else if (res->type == "fish") {
                                inventory->fishCount += res->amount;
                                std::cout << "[Gather] Gathered fish\n";
                            }
                            world->markForRemoval(entity);
                        }
                    }
                }
            }
            else if (inventory->activeSlot == 3) { // Spear
                std::cout << "[Combat] Attacked with Spear!\n";
                EventManager::emit("PLAYER_ATTACKED", 25);
            }
        }
    }
}
