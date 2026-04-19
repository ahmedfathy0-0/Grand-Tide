#pragma once

#include "ecs/world.hpp"
#include "components/shark-component.hpp"
#include "components/health.hpp"
#include "ecs/transform.hpp"
#include <glm/glm.hpp>
#include <iostream>
#include <cstdlib>

namespace our {

    class SharkSystem {
    public:
        void update(World* world, float deltaTime) {
            // Find the raft
            Entity* raft = nullptr;
            for (auto entity : world->getEntities()) {
                if (entity->name == "raft") {
                    raft = entity;
                    break;
                }
            }

            if (!raft) return;

            // Update all sharks
            for (auto entity : world->getEntities()) {
                auto shark = entity->getComponent<SharkComponent>();
                if (!shark) continue;

                auto health = entity->getComponent<HealthComponent>();
                if (health && health->currentHealth <= 0.0f) {
                    if (shark->state != SharkState::DEAD) {
                        shark->state = SharkState::DEAD;
                        world->markForRemoval(entity);
                    }
                    continue;
                }

                if (shark->damageFlashTimer > 0.0f) {
                    shark->damageFlashTimer -= deltaTime;
                }

                glm::vec3 sharkPos = entity->localTransform.position;
                glm::vec3 raftPosLocal = raft->localTransform.position;

                float dist = glm::distance(glm::vec3(sharkPos.x, 0.0f, sharkPos.z), glm::vec3(raftPosLocal.x, 0.0f, raftPosLocal.z));

                switch (shark->state) {
                    case SharkState::APPROACHING: {
                        shark->currentAnimIndex = 4; // Swim animation
                        
                        glm::vec3 dir = glm::vec3(raftPosLocal.x, 0.0f, raftPosLocal.z) - glm::vec3(sharkPos.x, 0.0f, sharkPos.z);
                        if (glm::length(dir) > 0.0001f) {
                            dir = glm::normalize(dir);
                            entity->localTransform.position += dir * shark->speed * deltaTime;
                            // Calculate yaw angle towards raft
                            entity->localTransform.rotation.y = atan2(-dir.z, dir.x) + glm::radians(90.0f); 
                            entity->localTransform.rotation.x = 0; // Ensure it is not tilted
                        }

                        if (dist < 25.0f) {
                            shark->state = SharkState::ATTACKING;
                            shark->currentAnimIndex = 0; // Bite animation
                            shark->stateTimer = 0.0f;
                        }
                        break;
                    }
                    case SharkState::ATTACKING: {
                        // Tilt up
                        entity->localTransform.rotation.x = glm::radians(-30.0f);
                        shark->stateTimer += deltaTime;

                        // Deal damage exactly once during attack
                        if (shark->stateTimer >= 0.5f && (shark->stateTimer - deltaTime) < 0.5f) {
                            auto raftHealth = raft->getComponent<HealthComponent>();
                            if (raftHealth) {
                                raftHealth->takeDamage(100.0f);
                                std::cout << "Shark attacked! Dealt 100 damage. Ship health: " << raftHealth->currentHealth << std::endl;
                            }
                        }

                        // Attack animation duration approx 1.5s
                        if (shark->stateTimer > 1.5f) {
                            shark->state = SharkState::SUBMERGED;
                            shark->stateTimer = 0.0f;
                        }
                        break;
                    }
                    case SharkState::SUBMERGED: {
                        entity->localTransform.position.y -= 10.0f * deltaTime; // Dive downwards
                        shark->stateTimer += deltaTime;

                        if (shark->stateTimer > 2.0f) {
                            // Respawn at a random far distance
                            float angle = static_cast<float>(rand() % 360) * 3.14159f / 180.0f;
                            float spawnDist = 100.0f; // updated from 40.0f to 200.0f
                            entity->localTransform.position = glm::vec3(
                                raftPosLocal.x + cos(angle) * spawnDist,
                                -7.0f, // updated from -1.0f to -10.0f
                                raftPosLocal.z + sin(angle) * spawnDist
                            );
                            entity->localTransform.rotation.x = 0.0f; // reset pitch
                            shark->state = SharkState::APPROACHING;
                            shark->stateTimer = 0.0f;
                        }
                        break;
                    }
                    case SharkState::DEAD: {
                        world->markForRemoval(entity);
                        break;
                    }
                }
            }
        }
    };

}
