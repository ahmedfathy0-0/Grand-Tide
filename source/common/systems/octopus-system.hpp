#pragma once

#include "ecs/world.hpp"
#include "components/octopus-component.hpp"
#include "components/health.hpp"
#include "components/animator.hpp"
#include "ecs/transform.hpp"
#include "application.hpp"
#include "mesh/model.hpp"
// #include "mesh/model-loader.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <iostream>

namespace our {

    class OctopusSystem {
    public:
        void update(World* world, float deltaTime, Application* app) {
            // Find the raft and player entities
            Entity* raft = nullptr;
            Entity* player = nullptr;
            for (auto entity : world->getEntities()) {
                if (entity->name == "raft") raft = entity;
                if (entity->name == "player") player = entity;
            }

            if (!raft) return;

            // Update all octopuses
            for (auto entity : world->getEntities()) {
                auto octopus = entity->getComponent<OctopusComponent>();
                if (!octopus) continue;

                glm::vec3& pos = entity->localTransform.position;

                // Determine target Y position based on state
                float targetY = octopus->submergedY;

                switch (octopus->state) {
                    case OctopusState::IDLE: {
                        // Under water, waiting to emerge
                        octopus->currentAnimIndex = 0;
                        targetY = octopus->submergedY;
                        octopus->stateTimer += deltaTime;
                        if (octopus->stateTimer >= octopus->emergeDelay) {
                            octopus->state = OctopusState::RISING;
                            octopus->currentAnimIndex = 12; // Emerge animation
                            octopus->stateTimer = 0.0f;
                            // Reset animation time for smooth emerge anim
                            auto animator = entity->getComponent<AnimatorComponent>();
                            if (animator) {
                                animator->currentAnimIndex = 12;
                                animator->currentAnimationTime = 0.0f;
                            }
                        }
                        break;
                    }
                    case OctopusState::RISING: {
                        // Emerging from water with animation 12
                        octopus->currentAnimIndex = 12;
                        targetY = octopus->surfacedY;
                        
                        // Track time spent in RISING to detect when animation 12 completes (~5s)
                        octopus->stateTimer += deltaTime;
                        if (octopus->stateTimer >= 5.0f) {
                            octopus->state = OctopusState::SURFACED_IDLE;
                            octopus->currentAnimIndex = 0; // Idle animation
                            octopus->stateTimer = 0.0f;
                            auto animator = entity->getComponent<AnimatorComponent>();
                            if (animator) {
                                animator->currentAnimIndex = 0;
                                animator->currentAnimationTime = 0.0f;
                            }
                        }
                        break;
                    }
                    case OctopusState::SURFACED_IDLE: {
                        // On surface, idle, tracking player
                        octopus->currentAnimIndex = 0;
                        targetY = octopus->surfacedY;
                        octopus->stateTimer += deltaTime;
                        if (octopus->stateTimer >= octopus->idleDuration) {
                            octopus->state = OctopusState::ATTACKING;
                            // Alternate between attack animation 1 and 2 based on toggle
                            int attackAnim = octopus->useAttackAnim2 ? 2 : 1;
                            octopus->currentAnimIndex = attackAnim;
                            octopus->stateTimer = 0.0f;
                            octopus->attackTimer = 0.0f;
                            auto animator = entity->getComponent<AnimatorComponent>();
                            if (animator) {
                                animator->currentAnimIndex = attackAnim;
                                animator->currentAnimationTime = 0.0f;
                            }
                        }
                        break;
                    }
                    case OctopusState::ATTACKING: {
                        // Attacking with animation 1 or 2 based on toggle
                        int attackAnim = octopus->useAttackAnim2 ? 2 : 1;
                        octopus->currentAnimIndex = attackAnim;
                        targetY = octopus->surfacedY;
                        octopus->stateTimer += deltaTime;
                        octopus->attackTimer += deltaTime;

                        // Calculate distance to player for hit detection and movement
                        float distToPlayer = 9999.0f;
                        glm::vec3 dirToPlayer = glm::vec3(0.0f);
                        if (player) {
                            glm::vec3 playerPos = player->localTransform.position;
                            glm::vec3 toPlayer = playerPos - pos;
                            toPlayer.y = 0.0f;
                            distToPlayer = glm::length(toPlayer);
                            if (distToPlayer > 0.01f) {
                                dirToPlayer = toPlayer / distToPlayer;
                            }
                        }

                        // If too far to hit, move towards player during attack
                        if (player && distToPlayer > octopus->attackRange) {
                            float moveSpeed = octopus->speed * deltaTime;
                            pos.x += dirToPlayer.x * moveSpeed;
                            pos.z += dirToPlayer.z * moveSpeed;
                        }

                        // Deal damage periodically only if within attack range
                        if (octopus->attackTimer >= octopus->attackCooldown) {
                            octopus->attackTimer = 0.0f;
                            
                            // Only deal damage if close enough to hit
                            if (distToPlayer <= octopus->attackRange) {
                                octopus->consecutiveMisses = 0; // Reset misses on hit
                                // Damage the raft
                                if (auto* raftHealth = raft->getComponent<HealthComponent>()) {
                                    raftHealth->takeDamage(octopus->attackDamage);
                                    std::cout << "[Octopus] Hit RAFT! Dealt " << octopus->attackDamage << " damage." << std::endl;
                                }
                                // Damage the player
                                if (player) {
                                    if (auto* playerHealth = player->getComponent<HealthComponent>()) {
                                        float playerDmg = octopus->attackDamage * 0.5f;
                                        playerHealth->takeDamage(playerDmg);
                                        std::cout << "[Octopus] HIT PLAYER! Dealt " << playerDmg << " damage. Player HP: " 
                                                  << playerHealth->currentHealth << "/" << playerHealth->maxHealth << std::endl;
                                        
                                        // Trigger damage flash on player
                                        playerHealth->damageFlashTimer = 3.0f; // 3 seconds of blood effect
                                    }
                                }
                            } else {
                                octopus->consecutiveMisses++;
                                std::cout << "[Octopus] Attack missed! Player too far (distance: " << distToPlayer 
                                          << ", range: " << octopus->attackRange << "). Misses: " << octopus->consecutiveMisses << std::endl;
                            }
                        }

                        // Stop attacking and walk closer if missed 3 times
                        if (octopus->consecutiveMisses >= 3) {
                            std::cout << "[Octopus] Missed 3 times! Moving closer..." << std::endl;
                            octopus->consecutiveMisses = 0;
                            octopus->followDistance = std::max(octopus->minFollowDistance, distToPlayer - 20.0f); // Adjust follow distance to get closer
                            
                            octopus->state = OctopusState::SURFACED_IDLE;
                            octopus->currentAnimIndex = 0;
                            octopus->stateTimer = 0.0f; // Reset to idle to trigger movement
                            
                            auto animator = entity->getComponent<AnimatorComponent>();
                            if (animator) {
                                animator->currentAnimIndex = 0;
                                animator->currentAnimationTime = 0.0f;
                            }
                            break;
                        }

                        // After attack duration, return to surfaced idle
                        if (octopus->stateTimer >= octopus->attackDuration) {
                            octopus->state = OctopusState::SURFACED_IDLE;
                            octopus->currentAnimIndex = 0;
                            octopus->stateTimer = 0.0f;
                            
                            // Increment attack count and toggle animation after 5 attacks
                            octopus->attackCount++;
                            if (octopus->attackCount >= 5) {
                                octopus->attackCount = 0;
                                octopus->useAttackAnim2 = !octopus->useAttackAnim2; // Toggle between anim 1 and 2
                                std::cout << "[Octopus] Switching to attack animation " << (octopus->useAttackAnim2 ? "2" : "1") 
                                          << " for next phase!" << std::endl;
                            }
                            
                            auto animator = entity->getComponent<AnimatorComponent>();
                            if (animator) {
                                animator->currentAnimIndex = 0;
                                animator->currentAnimationTime = 0.0f;
                            }
                        }
                        break;
                    }
                    case OctopusState::DEAD: {
                        world->markForRemoval(entity);
                        break;
                    }
                    case OctopusState::TESTING: {
                        targetY = octopus->surfacedY;
                        octopus->stateTimer = 0.0f;

                        // Keyboard input for animation testing
                        Keyboard& keyboard = app->getKeyboard();
                        int oldAnim = octopus->currentAnimIndex;
                        if (keyboard.justPressed(GLFW_KEY_Z)) octopus->currentAnimIndex = 0;
                        if (keyboard.justPressed(GLFW_KEY_X)) octopus->currentAnimIndex = 1;
                        if (keyboard.justPressed(GLFW_KEY_C)) octopus->currentAnimIndex = 2;
                        if (keyboard.justPressed(GLFW_KEY_V)) octopus->currentAnimIndex = 3;
                        if (keyboard.justPressed(GLFW_KEY_B)) octopus->currentAnimIndex = 4;
                        if (keyboard.justPressed(GLFW_KEY_N)) octopus->currentAnimIndex = 5;
                        if (keyboard.justPressed(GLFW_KEY_M)) octopus->currentAnimIndex = 6;
                        if (keyboard.justPressed(GLFW_KEY_COMMA)) octopus->currentAnimIndex = 7;
                        if (keyboard.justPressed(GLFW_KEY_PERIOD)) octopus->currentAnimIndex = 8;
                        if (keyboard.justPressed(GLFW_KEY_SLASH)) octopus->currentAnimIndex = 9;
                        if (oldAnim != octopus->currentAnimIndex) {
                            std::cout << "Switched to Animation: " << octopus->currentAnimIndex << std::endl;
                        }
                        break;
                    }
                }

                // --- Position: track player but don't retreat when too close ---
                // Only track distance after fully surfaced (not during IDLE/RISING)
                if (player && octopus->state != OctopusState::IDLE && octopus->state != OctopusState::RISING) {
                    glm::vec3 playerPos = player->localTransform.position;
                    glm::vec3 toPlayer = playerPos - pos;
                    toPlayer.y = 0.0f;
                    float distToPlayer = glm::length(toPlayer);

                    if (distToPlayer > 0.01f) {
                        glm::vec3 dirToPlayer = toPlayer / distToPlayer;

                        // Only move when too far away - never retreat when too close
                        if (distToPlayer > octopus->maxFollowDistance) {
                            // Too far: move towards the player slowly to reach followDistance
                            glm::vec3 targetXZ = playerPos - dirToPlayer * octopus->followDistance;
                            // Use slower speed for more natural movement
                            float approachSpeed = octopus->speed * 0.5f; // Half speed for approach
                            pos.x += (targetXZ.x - pos.x) * approachSpeed * deltaTime;
                            pos.z += (targetXZ.z - pos.z) * approachSpeed * deltaTime;
                        }
                        // If too close: do nothing - don't push away, stay put
                    }
                }

                // Smoothly interpolate Y position
                pos.y += (targetY - pos.y) * 2.0f * deltaTime;

                // --- Rotation: always face the player ---
                if (player) {
                    glm::vec3 playerPos = player->localTransform.position;
                    glm::vec3 diff = playerPos - pos;
                    diff.y = 0.0f; // Only rotate on Y axis
                    if (glm::length(diff) > 0.01f) {
                        float targetYaw = atan2(diff.x, diff.z);
                        entity->localTransform.rotation.y = targetYaw;
                    }
                }
                entity->localTransform.rotation.x = 0.0f;
                entity->localTransform.rotation.z = 0.0f;

                // --- Update animator ---
                auto animator = entity->getComponent<AnimatorComponent>();
                if (animator) {
                    animator->currentAnimIndex = octopus->currentAnimIndex;
                }
            }
        }
    };

}
