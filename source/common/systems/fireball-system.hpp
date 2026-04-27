#pragma once

#include "ecs/world.hpp"
#include "components/camera.hpp"
#include "components/player-controller.hpp"
#include "components/inventory.hpp"
#include "components/fireball-component.hpp"
#include "components/projectile-component.hpp"
#include "components/burn-component.hpp"
#include "components/mesh-renderer.hpp"
#include "components/animator.hpp"
#include "components/health.hpp"
#include "components/enemy.hpp"
#include "components/marine-boat-component.hpp"
#include "components/shark-component.hpp"
#include "components/musket-component.hpp"
#include "application.hpp"
#include "mesh/model.hpp"
#include "asset-loader.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <iostream>

namespace our {

    class FireballSystem {
        Application* app = nullptr;

        // Track the hand entity and fire-in-hand entity so we can toggle visibility
        Entity* handEntity = nullptr;
        Entity* fireInHandEntity = nullptr;
        Entity* aoeIndicatorEntity = nullptr;
        Entity* weaponEntity = nullptr;

        // Devil fruit tracking
        bool devilFruitSpawned = false;
        Entity* devilFruitEntity = nullptr;
        float devilFruitVelocityY = 0.0f;
        float devilFruitMessageTimer = 0.0f;
        bool showDevilFruitMessage = false;

    public:
        void enter(Application* app) {
            this->app = app;
        }

        bool shouldShowDevilFruitMessage() const { return showDevilFruitMessage; }

        void update(World* world, float deltaTime) {
            Entity* playerEntity = nullptr;
            FireballComponent* fireball = nullptr;

            // Find player and fireball component
            for (auto entity : world->getEntities()) {
                if (entity->name == "player") playerEntity = entity;
                auto fb = entity->getComponent<FireballComponent>();
                if (fb) {
                    fireball = fb;
                    // Track the hand entity (the entity that has FireballComponent)
                    handEntity = entity;
                }
                if (entity->name == "fire_in_hand") fireInHandEntity = entity;
                if (entity->name == "aoe_indicator") aoeIndicatorEntity = entity;
            }

            // Find the regular weapon entity (child of player) to hide when slot 5 is active
            for (auto entity : world->getEntities()) {
                if (entity->parent == playerEntity && entity->name == "weapon") {
                    weaponEntity = entity;
                }
            }

            if (!playerEntity) return;

            // --- Check if last shark died and spawn devil fruit ---
            // (This runs regardless of whether player has fireball yet)
            if (!devilFruitSpawned) {
                bool anySharkAlive = false;
                int sharkCount = 0;
                int deadSharkCount = 0;
                for (auto entity : world->getEntities()) {
                    auto shark = entity->getComponent<SharkComponent>();
                    auto enemy = entity->getComponent<EnemyComponent>();
                    if (shark) {
                        sharkCount++;
                        if (enemy && enemy->state != EnemyState::DEAD) {
                            anySharkAlive = true;
                        } else if (enemy && enemy->state == EnemyState::DEAD) {
                            deadSharkCount++;
                        }
                    }
                }
                std::cout << "[DevilFruit] sharks=" << sharkCount
                          << " alive=" << (anySharkAlive ? "yes" : "no")
                          << " dead=" << deadSharkCount << "\n";
                if (!anySharkAlive && sharkCount > 0) {
                    std::cout << "[DevilFruit] Spawning devil fruit!\n";
                    spawnDevilFruit(world, playerEntity);
                    devilFruitSpawned = true;
                }
            }

            // --- Devil fruit physics (gravity) ---
            if (devilFruitEntity) {
                devilFruitVelocityY -= 9.8f * deltaTime; // gravity
                devilFruitEntity->localTransform.position.y += devilFruitVelocityY * deltaTime;
                // Stop at water level (y = 0.5)
                if (devilFruitEntity->localTransform.position.y < 0.5f) {
                    devilFruitEntity->localTransform.position.y = 0.5f;
                    devilFruitVelocityY = 0.0f;
                }
            }

            // --- Devil fruit pickup (press E when near) ---
            if (devilFruitEntity && app->getKeyboard().justPressed(GLFW_KEY_E)) {
                float dist = glm::distance(playerEntity->localTransform.position, devilFruitEntity->localTransform.position);
                if (dist < 3.0f) {
                    auto inventory = playerEntity->getComponent<InventoryComponent>();
                    if (inventory) {
                        inventory->hasDevilFruit = true;
                        showDevilFruitMessage = true;
                        devilFruitMessageTimer = 5.0f; // show for 5 seconds
                        world->markForRemoval(devilFruitEntity);
                        devilFruitEntity = nullptr;
                    }
                }
            }

            // --- Devil fruit message timer ---
            if (showDevilFruitMessage) {
                devilFruitMessageTimer -= deltaTime;
                if (devilFruitMessageTimer <= 0.0f) {
                    showDevilFruitMessage = false;
                }
            }

            // Early return for fireball-specific code only
            if (!fireball) return;

            auto inventory = playerEntity->getComponent<InventoryComponent>();
            if (!inventory) return;

            // --- Slot 5 key binding (only if has devil fruit) ---
            if (app->getKeyboard().justPressed(GLFW_KEY_5)) {
                if (inventory->hasDevilFruit) {
                    inventory->activeSlot = 5;
                }
            }

            bool isSlot5 = (inventory->activeSlot == 5);

            // --- Visibility: show throw_hand and hide regular weapon when slot 5 is active ---
            if (handEntity) {
                handEntity->localTransform.scale = isSlot5 ? glm::vec3(0.5f) : glm::vec3(0.0f);
            }
            if (weaponEntity) {
                weaponEntity->localTransform.scale = isSlot5 ? glm::vec3(0.0f) : glm::vec3(0.1f, 0.1f, 1.0f);
            }

            // Get the Animator on the hand so we can control the throw animation
            AnimatorComponent* handAnimator = nullptr;
            if (handEntity) {
                handAnimator = handEntity->getComponent<AnimatorComponent>();
            }

            // --- Cooldown ---
            if (fireball->cooldownTimer > 0.0f) {
                fireball->cooldownTimer -= deltaTime;
            }

            // --- State machine ---
            switch (fireball->state) {

            case FireballState::IDLE: {
                // No animation when idle (static T-pose / bind pose)
                if (handAnimator) handAnimator->currentAnimIndex = -1;

                // Hide fire in hand and AOE indicator
                if (fireInHandEntity) fireInHandEntity->localTransform.scale = glm::vec3(0.0f);
                if (aoeIndicatorEntity) aoeIndicatorEntity->localTransform.scale = glm::vec3(0.0f);

                // If slot 5 active and right-click pressed, start aiming
                if (isSlot5 && app->getMouse().isPressed(GLFW_MOUSE_BUTTON_RIGHT) && fireball->cooldownTimer <= 0.0f) {
                    fireball->state = FireballState::AIMING;
                }
                break;
            }

            case FireballState::AIMING: {
                if (!isSlot5) {
                    fireball->state = FireballState::IDLE;
                    break;
                }

                // Still idle pose while aiming (no throw animation yet)
                if (handAnimator) handAnimator->currentAnimIndex = -1;

                // Show fire in hand
                if (fireInHandEntity) {
                    fireInHandEntity->localTransform.scale = glm::vec3(0.15f);
                }

                // Compute aim target: raycast from camera forward onto y=0 plane
                computeAimTarget(playerEntity, fireball);

                // Show AOE indicator at aim target
                if (aoeIndicatorEntity && fireball->hasAimTarget) {
                    aoeIndicatorEntity->localTransform.position = fireball->aimTarget;
                    aoeIndicatorEntity->localTransform.scale = glm::vec3(fireball->aoeRadius * 2.0f, fireball->aoeRadius * 2.0f, fireball->aoeRadius * 2.0f);
                    aoeIndicatorEntity->localTransform.rotation = glm::vec3(glm::radians(90.0f), 0.0f, 0.0f);
                }

                // If right-click released, go back to idle
                if (!app->getMouse().isPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
                    fireball->state = FireballState::IDLE;
                    break;
                }

                // If Left mouse button pressed while aiming, throw!
                if (app->getMouse().justPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                    spawnProjectile(world, playerEntity, fireball);
                    fireball->state = FireballState::THROWING;
                    fireball->cooldownTimer = fireball->cooldown;
                    fireball->throwAnimTimer = fireball->throwAnimDuration; // start throw anim

                    // Hide fire in hand and AOE indicator
                    if (fireInHandEntity) fireInHandEntity->localTransform.scale = glm::vec3(0.0f);
                    if (aoeIndicatorEntity) aoeIndicatorEntity->localTransform.scale = glm::vec3(0.0f);
                }
                break;
            }

            case FireballState::THROWING: {
                // Play throw animation while timer is active, then return to idle pose
                if (handAnimator) {
                    if (fireball->throwAnimTimer > 0.0f) {
                        handAnimator->currentAnimIndex = 0;   // Throw animation!
                    } else {
                        handAnimator->currentAnimIndex = -1;  // Back to idle pose
                    }
                }

                // Tick throw animation timer
                if (fireball->throwAnimTimer > 0.0f) {
                    fireball->throwAnimTimer -= deltaTime;
                }

                // Wait for cooldown then back to idle
                if (fireball->cooldownTimer <= 0.0f) {
                    fireball->state = FireballState::IDLE;
                    if (handAnimator) handAnimator->currentAnimIndex = -1;
                }
                break;
            }
            }

            // --- Update projectiles ---
            updateProjectiles(world, deltaTime, playerEntity);

            // --- Update burning entities ---
            updateBurning(world, deltaTime, playerEntity);

            // --- Devil fruit message timer ---
            if (showDevilFruitMessage && devilFruitMessageTimer > 0.0f) {
                devilFruitMessageTimer -= deltaTime;
                if (devilFruitMessageTimer <= 0.0f) {
                    showDevilFruitMessage = false;
                }
            }
        }

    private:
        void computeAimTarget(Entity* player, FireballComponent* fireball) {
            // Get camera forward direction and do a simple ray-plane intersection at y=0
            auto camera = player->getComponent<CameraComponent>();
            if (!camera) {
                // Fallback: use player forward
                glm::mat4 mat = player->localTransform.toMat4();
                glm::vec3 forward = glm::normalize(glm::vec3(mat * glm::vec4(0, 0, -1, 0)));
                glm::vec3 start = player->localTransform.position + glm::vec3(0, 1.5f, 0);
                if (forward.y < -0.01f) {
                    float t = -start.y / forward.y;
                    fireball->aimTarget = start + forward * t;
                    fireball->hasAimTarget = true;
                }
                return;
            }

            // Get camera entity
            Entity* camEntity = camera->getOwner();
            glm::mat4 camMat = camEntity->getLocalToWorldMatrix();
            glm::vec3 camPos = glm::vec3(camMat * glm::vec4(0, 0, 0, 1));
            glm::vec3 camFwd = glm::normalize(glm::vec3(camMat * glm::vec4(0, 0, -1, 0)));

            // Ray-plane intersection with y=0
            if (camFwd.y < -0.01f) {
                float t = -camPos.y / camFwd.y;
                if (t > 0.0f) {
                    fireball->aimTarget = camPos + camFwd * t;
                    fireball->hasAimTarget = true;
                    return;
                }
            }
            // Fallback: project forward at a fixed distance
            fireball->aimTarget = camPos + camFwd * 20.0f;
            fireball->aimTarget.y = 0.0f;
            fireball->hasAimTarget = true;
        }

        void spawnProjectile(World* world, Entity* player, FireballComponent* fireball) {
            if (!fireball->hasAimTarget) return;

            Entity* projectile = world->add();
            projectile->name = "fireball_projectile";

            // Start position: player position + offset up
            glm::vec3 startPos = player->localTransform.position + glm::vec3(0.0f, 1.8f, 0.0f);

            // Direction toward aim target with arc
            glm::vec3 target = fireball->aimTarget + glm::vec3(0.0f, 0.5f, 0.0f); // Slightly above ground
            glm::vec3 direction = target - startPos;
            float distance = glm::length(direction);
            if (distance > 0.01f) direction = glm::normalize(direction);

            // Add upward arc for grenade-like trajectory
            glm::vec3 velocity = direction * fireball->throwSpeed;
            velocity.y += 10.0f; // Arc upward for longer range

            projectile->localTransform.position = startPos;
            projectile->localTransform.scale = glm::vec3(0.6f);

            // Add projectile component
            auto projComp = projectile->addComponent<ProjectileComponent>();
            projComp->velocity = velocity;
            projComp->aoeRadius = fireball->aoeRadius;
            projComp->burnDuration = fireball->burnDuration;
            projComp->burnDamagePerSecond = fireball->burnDamagePerSecond;

            // Add mesh renderer for the fireball (sphere with fire material)
            auto renderer = projectile->addComponent<MeshRendererComponent>();
            renderer->mesh = AssetLoader<Mesh>::get("sphere");
            renderer->material = AssetLoader<Material>::get("fire_mat");
        }

        // Billboard: rotate entity to face the camera (Y-axis only so it stays upright)
        void billboardToCamera(Entity* entity, Entity* playerEntity) {
            auto camera = playerEntity->getComponent<CameraComponent>();
            if (!camera) return;
            Entity* camEntity = camera->getOwner();
            glm::vec3 camPos = glm::vec3(camEntity->getLocalToWorldMatrix() * glm::vec4(0, 0, 0, 1));

            glm::vec3 toCamera = camPos - entity->localTransform.position;
            toCamera.y = 0; // keep upright, only yaw rotation

            if (glm::length(toCamera) > 0.01f) {
                float yaw = atan2(toCamera.x, toCamera.z);
                entity->localTransform.rotation.y = yaw;
            }
        }

        void spawnDevilFruit(World* world, Entity* playerEntity) {
            // Find raft position
            Entity* raft = nullptr;
            for (auto entity : world->getEntities()) {
                if (entity->name == "raft") {
                    raft = entity;
                    break;
                }
            }
            glm::vec3 spawnPos = glm::vec3(0.0f, 10.f, 10.0f); // default: center of world + above water
            if (raft) {
                glm::vec3 raftPos = raft->localTransform.position;
                spawnPos = raftPos + glm::vec3(0.0f, 20.0f, 0.0f); // center of raft, high above
            }
            std::cout << "[DevilFruit] Spawned at: " << spawnPos.x << ", " << spawnPos.y << ", " << spawnPos.z << "\n";

            // High pop-up then fall onto raft
            devilFruitVelocityY = 0.0f; // start at rest, already high enough

            devilFruitEntity = world->add();
            devilFruitEntity->name = "devil_fruit";
            devilFruitEntity->localTransform.position = spawnPos;
            devilFruitEntity->localTransform.scale = glm::vec3(0.8f); // bigger so you can see it
            devilFruitEntity->localTransform.rotation = glm::vec3(0.0f);

            auto renderer = devilFruitEntity->addComponent<MeshRendererComponent>();
            renderer->mesh = AssetLoader<Mesh>::get("firefruitupload");
            renderer->material = AssetLoader<Material>::get("devil_fruit_mat");

            // Add a BurnComponent for a subtle glow effect
            auto burn = devilFruitEntity->addComponent<BurnComponent>();
            burn->remainingTime = 9999.0f;
            burn->damagePerSecond = 0.0f;
            burn->burnIntensity = 0.5f;
        }

        void updateProjectiles(World* world, float deltaTime, Entity* playerEntity) {
            for (auto entity : world->getEntities()) {
                auto proj = entity->getComponent<ProjectileComponent>();
                if (!proj || !proj->active) continue;

                // Move projectile
                entity->localTransform.position += proj->velocity * deltaTime;

                // Apply gravity
                proj->velocity.y -= 9.8f * deltaTime;

                // Age projectile
                proj->age += deltaTime;

                // Billboard: make the fire quad face the camera
                billboardToCamera(entity, playerEntity);

                // Check collision with y=0 plane (water/ground)
                bool hit = entity->localTransform.position.y <= 0.3f;

                // Check collision with enemy entities
                if (!hit) {
                    for (auto target : world->getEntities()) {
                        auto enemy = target->getComponent<EnemyComponent>();
                        if (!enemy) continue;

                        glm::vec3 targetPos = target->localTransform.position;
                        float dist = glm::distance(glm::vec3(entity->localTransform.position.x, 0, entity->localTransform.position.z),
                                                    glm::vec3(targetPos.x, 0, targetPos.z));
                        // Use a generous collision radius based on entity scale
                        float collisionRadius = 3.0f;
                        if (dist < collisionRadius) {
                            hit = true;
                            break;
                        }
                    }
                }

                // Timeout
                if (proj->age >= proj->lifetime) hit = true;

                if (hit) {
                    // Ignite entities in AOE radius
                    glm::vec3 hitPos = entity->localTransform.position;
                    hitPos.y = 0.0f;

                    for (auto target : world->getEntities()) {
                        // Check if entity is burnable (has Health + is a boat or shark)
                        auto health = target->getComponent<HealthComponent>();
                        if (!health) continue;

                        // Must be a boat, shark, marine boat, or musket
                        bool isBurnable = target->getComponent<MarineBoatComponent>() != nullptr ||
                                         target->getComponent<SharkComponent>() != nullptr ||
                                         target->getComponent<MusketComponent>() != nullptr ||
                                         target->name == "raft" ||
                                         target->name == "boat";

                        if (!isBurnable) continue;

                        // Use world position for distance — muskets are child entities
                        glm::vec3 targetPos = glm::vec3(target->getLocalToWorldMatrix()[3]);
                        float dist = glm::distance(glm::vec3(hitPos.x, 0, hitPos.z),
                                                    glm::vec3(targetPos.x, 0, targetPos.z));

                        if (dist <= proj->aoeRadius) {
                            // Add burn component if not already burning
                            auto existingBurn = target->getComponent<BurnComponent>();
                            if (!existingBurn) {
                                auto burn = target->addComponent<BurnComponent>();
                                burn->remainingTime = proj->burnDuration;
                                burn->damagePerSecond = proj->burnDamagePerSecond;
                                burn->burnIntensity = 1.0f;
                            } else {
                                // Refresh burn duration
                                existingBurn->remainingTime = proj->burnDuration;
                                existingBurn->burnIntensity = 1.0f;
                            }
                        }
                    }

                    // Spawn AOE fire effect at hit location
                    spawnAOEFireEffect(world, hitPos, proj->aoeRadius, proj->burnDuration);

                    // Despawn projectile
                    proj->active = false;
                    world->markForRemoval(entity);
                }
            }
        }

        void spawnAOEFireEffect(World* world, glm::vec3 position, float radius, float duration) {
            // --- Tall vertical fire column (sphere stretched vertically with fire shader) ---
            Entity* fireColumn = world->add();
            fireColumn->name = "aoe_fire_effect";
            fireColumn->localTransform.position = position + glm::vec3(0.0f, radius * 0.8f, 0.0f);
            fireColumn->localTransform.scale = glm::vec3(radius * 1.5f, radius * 2.5f, radius * 1.5f);

            auto colRenderer = fireColumn->addComponent<MeshRendererComponent>();
            colRenderer->mesh = AssetLoader<Mesh>::get("plane");
            colRenderer->material = AssetLoader<Material>::get("fire_mat");

            auto colBurn = fireColumn->addComponent<BurnComponent>();
            colBurn->remainingTime = duration;
            colBurn->damagePerSecond = 0.0f;
            colBurn->burnIntensity = 1.0f;

            // --- Ground ring (flat circle on the water for AOE indication) ---
            Entity* groundRing = world->add();
            groundRing->name = "aoe_fire_effect";
            groundRing->localTransform.position = position + glm::vec3(0.0f, 0.1f, 0.0f);
            groundRing->localTransform.rotation = glm::vec3(glm::radians(90.0f), 0.0f, 0.0f);
            groundRing->localTransform.scale = glm::vec3(radius * 2.0f, radius * 2.0f, radius * 2.0f);

            auto ringRenderer = groundRing->addComponent<MeshRendererComponent>();
            ringRenderer->mesh = AssetLoader<Mesh>::get("plane");
            ringRenderer->material = AssetLoader<Material>::get("aoe_fire_mat");

            auto ringBurn = groundRing->addComponent<BurnComponent>();
            ringBurn->remainingTime = duration;
            ringBurn->damagePerSecond = 0.0f;
            ringBurn->burnIntensity = 1.0f;
        }

        void updateBurning(World* world, float deltaTime, Entity* playerEntity) {
            std::vector<Entity*> toRemoveBurnFrom;

            for (auto entity : world->getEntities()) {
                auto burn = entity->getComponent<BurnComponent>();
                if (!burn) continue;

                // Tick down remaining time
                burn->remainingTime -= deltaTime;

                // Fade out intensity near end
                if (burn->remainingTime < 1.0f) {
                    burn->burnIntensity = glm::max(0.0f, burn->remainingTime);
                }

                // Billboard visual-only fire columns to face the camera
                if (entity->name == "aoe_fire_effect" && burn->damagePerSecond == 0.0f && !entity->getComponent<HealthComponent>()) {
                    billboardToCamera(entity, playerEntity);
                }

                // Apply damage if this entity has health (skip visual-only burns)
                if (burn->damagePerSecond > 0.0f) {
                    auto health = entity->getComponent<HealthComponent>();
                    if (health && health->currentHealth > 0.0f) {
                        health->takeDamage(burn->damagePerSecond * deltaTime);
                    }
                }

                // Remove burn when expired
                if (burn->remainingTime <= 0.0f) {
                    toRemoveBurnFrom.push_back(entity);
                }
            }

            // Remove expired burns and their visual-only entities
            for (auto entity : toRemoveBurnFrom) {
                auto burn = entity->getComponent<BurnComponent>();
                if (burn) {
                    entity->deleteComponent<BurnComponent>();
                }
                // If this was a visual-only AOE fire effect, remove the entity
                if (entity->name == "aoe_fire_effect") {
                    world->markForRemoval(entity);
                }
            }
        }
    };

}
