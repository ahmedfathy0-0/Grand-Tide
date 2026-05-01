#include "survival-system.hpp"
#include "../asset-loader.hpp"
#include "../components/enemy.hpp"
#include "../components/animator.hpp"

#include <glm/gtx/euler_angles.hpp>

namespace our
{

    namespace
    {
        struct WeaponComponents
        {
            MeshRendererComponent *meshRenderer;
            AnimatorComponent *animator;
        };

        // Find weapon entity by name (world-level entity, not child of player)
        WeaponComponents getWeaponComponents(World *world)
        {
            WeaponComponents wc = {nullptr, nullptr};
            if (!world)
                return wc;
            for (auto entity : world->getEntities())
            {
                if (entity->name == "weapon")
                {
                    wc.meshRenderer = entity->getComponent<MeshRendererComponent>();
                    wc.animator = entity->getComponent<AnimatorComponent>();
                    break;
                }
            }
            return wc;
        }
    }

    void SurvivalSystem::handleBoatDamaged(int damage)
    {
        if (!boatEntity)
            return;

        auto boatHealth = boatEntity->getComponent<HealthComponent>();
        if (!boatHealth)
            return;

        boatHealth->takeDamage(static_cast<float>(damage));
        std::cout << "[Event] Boat damaged for " << damage << " hp. Current: " << boatHealth->currentHealth << "\n";
        if (boatHealth->isDead())
        {
            std::cout << "[Event] Boat destroyed!\n";
            auto meshR = boatEntity->getComponent<MeshRendererComponent>();
            if (meshR)
            {
                meshR->material = AssetLoader<Material>::get("dark_broken");
            }
        }
    }

    void SurvivalSystem::handleRepairAction()
    {
        if (!playerEntity || !boatEntity)
            return;

        auto inventory = playerEntity->getComponent<InventoryComponent>();
        auto boatHealth = boatEntity->getComponent<HealthComponent>();
        if (!inventory || !boatHealth)
            return;

        if (inventory->activeSlot != 1 || inventory->woodCount <= 0)
            return;
        if (boatHealth->currentHealth >= boatHealth->maxHealth)
            return;

        const glm::vec3 playerPos = glm::vec3(playerEntity->getLocalToWorldMatrix()[3]);
        const glm::vec3 boatPos = glm::vec3(boatEntity->getLocalToWorldMatrix()[3]);
        const float dist = glm::distance(playerPos, boatPos);
        if (dist > 4.5f)
            return;

        boatHealth->heal(100.0f);
        inventory->woodCount--;
        std::cout << "[Repair] Boat repaired! Wood left: " << inventory->woodCount << "\n";

        auto meshR = boatEntity->getComponent<MeshRendererComponent>();
        if (meshR)
        {
            meshR->material = AssetLoader<Material>::get("raft_mat");
        }
    }

    void SurvivalSystem::handleGatherAction()
    {
        if (!world || !playerEntity)
            return;

        auto inventory = playerEntity->getComponent<InventoryComponent>();
        if (!inventory || inventory->activeSlot != 2)
            return;

        const glm::vec3 playerPos = glm::vec3(playerEntity->getLocalToWorldMatrix()[3]);

        for (auto entity : world->getEntities())
        {
            auto res = entity->getComponent<ResourceComponent>();
            if (!res)
                continue;

            const glm::vec3 resPos = glm::vec3(entity->getLocalToWorldMatrix()[3]);
            const float dist = glm::distance(playerPos, resPos);
            if (dist >= 15.0f)
                continue;

            if (res->type == "wood")
            {
                inventory->woodCount += res->amount;
                std::cout << "[Gather] Gathered wood\n";
            }
            else if (res->type == "fish")
            {
                inventory->fishCount += res->amount;
                std::cout << "[Gather] Gathered fish\n";
            }
            world->markForRemoval(entity);
        }
    }

    void SurvivalSystem::handleAttackAction()
    {
        if (!world || !playerEntity)
            return;
        auto inventory = playerEntity->getComponent<InventoryComponent>();
        if (!inventory || inventory->activeSlot != 3)
            return;

        std::cout << "[Combat] Attacked with Spear!\n";
        EventManager::emit("PLAYER_ATTACKED", 25);

        const glm::vec3 playerPos = glm::vec3(playerEntity->getLocalToWorldMatrix()[3]);

        for (auto entity : world->getEntities())
        {
            auto enemy = entity->getComponent<EnemyComponent>();
            auto health = entity->getComponent<HealthComponent>();
            if (!enemy || !health || health->isDead())
                continue;

            glm::vec3 targetPos = glm::vec3(entity->getLocalToWorldMatrix()[3]);

            // Player's look direction (forward is -Z in local space)
            glm::vec3 playerForward = glm::normalize(glm::vec3(playerEntity->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));

            // Target bounding radius (Shark is very long, so we give it a large bounding bubble to cover snout to tail)
            float targetRadius = (enemy && enemy->type == EnemyType::SHARK) ? 10.0f : 2.0f;
            float spearReach = 10.0f; // Spear thrust distance

            // Find the closest point along the spear's line segment to the target's core
            glm::vec3 diff = targetPos - playerPos;
            float t = glm::clamp(glm::dot(diff, playerForward), 0.0f, spearReach);
            glm::vec3 closestSpearPoint = playerPos + playerForward * t;

            float distToTarget = glm::distance(closestSpearPoint, targetPos);

            if (distToTarget <= targetRadius)
            {
                health->takeDamage(25.0f);
                if (entity->getComponent<EnemyComponent>()->type == EnemyType::SHARK)
                    entity->getComponent<SharkComponent>()->damageFlashTimer = 0.5f;
                std::cout << "[Combat] Hit entity for 25 damage! Health left: " << health->currentHealth << "\n";
            }
        }
    }

    void SurvivalSystem::playPainSound() {
        if (!painSoundEngine) {
            painSoundEngine = new ma_engine();
            if (ma_engine_init(nullptr, painSoundEngine) != MA_SUCCESS) {
                std::cerr << "[Survival] Pain sound engine init failed" << std::endl;
                delete painSoundEngine;
                painSoundEngine = nullptr;
                return;
            }
        }
        ma_engine_play_sound(painSoundEngine, "assets/audios/pain.mp3", nullptr);
    }

    void SurvivalSystem::update()
    {
        if (!playerEntity || !boatEntity)
            return;

        auto inventory = playerEntity->getComponent<InventoryComponent>();
        auto playerHealth = playerEntity->getComponent<HealthComponent>();
        if (!inventory || !playerHealth)
            return;

        // Check if player was damaged this frame
        if (playerHealth->justDamaged) {
            playPainSound();
            playerHealth->justDamaged = false;
        }

        auto &keyboard = app->getKeyboard();
        auto &mouse = app->getMouse();

        // 1. Tool Switching
        if (keyboard.justPressed(GLFW_KEY_1))
        {
            inventory->activeSlot = 1;
            std::cout << "[Tool] Switched to Hammer\n";
            auto wc = getWeaponComponents(world);
            if (wc.meshRenderer)
            {
                wc.meshRenderer->mesh = AssetLoader<Mesh>::get("cube");
                wc.meshRenderer->material = AssetLoader<Material>::get("lit_hammer");
            }
            if (wc.animator)
                wc.animator->modelName = "hammer";
            activeToolTransform = &hammerTransform;
            if (weaponEntity)
                weaponEntity->localTransform.scale = hammerTransform.scale;
        }
        if (keyboard.justPressed(GLFW_KEY_2))
        {
            inventory->activeSlot = 2;
            std::cout << "[Tool] Switched to Net\n";
            auto wc = getWeaponComponents(world);
            if (wc.meshRenderer)
            {
                wc.meshRenderer->mesh = AssetLoader<Mesh>::get("cube");
                wc.meshRenderer->material = AssetLoader<Material>::get("lit_net");
            }
            if (wc.animator)
                wc.animator->modelName = "net";
            activeToolTransform = &netTransform;
            if (weaponEntity)
                weaponEntity->localTransform.scale = netTransform.scale;
        }
        if (keyboard.justPressed(GLFW_KEY_3))
        {
            inventory->activeSlot = 3;
            std::cout << "[Tool] Switched to Spear\n";
            auto wc = getWeaponComponents(world);
            if (wc.meshRenderer)
            {
                wc.meshRenderer->mesh = AssetLoader<Mesh>::get("spear");
                wc.meshRenderer->material = AssetLoader<Material>::get("lit_spear");
            }
            if (wc.animator)
                wc.animator->modelName = ""; // Static
            activeToolTransform = &spearTransform;
            if (weaponEntity)
                weaponEntity->localTransform.scale = spearTransform.scale;
        }
        if (keyboard.justPressed(GLFW_KEY_5))
        {
            if (inventory->hasDevilFruit) {
                inventory->activeSlot = 5;
                std::cout << "[Tool] Switched to Fireball\n";
                // Hide weapon when using fireball slot
                if (weaponEntity)
                    weaponEntity->localTransform.scale = glm::vec3(0.0f);
            } else {
                std::cout << "[Tool] You don't have devil fruit powers yet!\n";
            }
        }

        // --- Weapon follow-player logic ---
        // Make the weapon a child of the player entity so the ECS transform hierarchy
        // automatically handles yaw + pitch composition (no manual euler angle math needed).
        if (weaponEntity && playerEntity && activeToolTransform)
        {
            // Parent the weapon to the player (only needs to be set once)
            if (weaponEntity->parent != playerEntity)
                weaponEntity->parent = playerEntity;

            // Don't move/show weapon if fireball slot is active
            if (inventory->activeSlot != 5)
            {
                // Set local transform relative to the player — the hierarchy does the rest.
                // Compensate for parent scale: divide desired world scale by parent scale
                // so the final composed scale matches the ToolConfig values.
                glm::vec3 parentScale = playerEntity->localTransform.scale;
                weaponEntity->localTransform.position = activeToolTransform->localPosition;
                weaponEntity->localTransform.rotation = activeToolTransform->localRotation;
                weaponEntity->localTransform.scale    = activeToolTransform->scale / parentScale;
            }
        }

        // 2. Consuming
        if (keyboard.justPressed(GLFW_KEY_R))
        {
            if (inventory->fishCount > 0 && playerHealth->currentHealth < playerHealth->maxHealth)
            {
                playerHealth->heal(20.0f);
                inventory->fishCount--;
                std::cout << "[Consume] Ate fish. Health now: " << playerHealth->currentHealth << "\n";
            }
        }

        // 3. Debug Damage to Boat
        if (keyboard.justPressed(GLFW_KEY_K))
        {
            EventManager::emit("BOAT_DAMAGED", 100);
        }

        // 4. Tool Actions (event-driven requests)
        if (mouse.justPressed(GLFW_MOUSE_BUTTON_1))
        {
            if (inventory->activeSlot == 1)
            {
                EventManager::emit("PLAYER_REPAIR_ACTION");
            }
            else if (inventory->activeSlot == 2)
            {
                EventManager::emit("PLAYER_GATHER_ACTION");
            }
            else if (inventory->activeSlot == 3)
            {
                EventManager::emit("PLAYER_ATTACK_ACTION");
            }
        }
    }
}
