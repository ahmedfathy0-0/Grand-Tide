#pragma once

#include "ecs/world.hpp"
#include "components/shark-component.hpp"
#include "components/health.hpp"
#include "ecs/transform.hpp"
#include <glm/glm.hpp>
#include <iostream>

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

            glm::vec3 raftPos = raft->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

            // Update all sharks
            for (auto entity : world->getEntities()) {
                auto shark = entity->getComponent<SharkComponent>();
                if (!shark) continue;

                auto health = entity->getComponent<HealthComponent>();
                if (health && health->currentHealth <= 0.0f) {
                    if (shark->state != SharkState::DEAD) {
                        shark->state = SharkState::DEAD;
                        // To make it disappear as user requested:
                        world->markForRemoval(entity);
                    }
                    continue;
                }

                glm::vec3 sharkPos = entity->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                float dist = glm::distance(sharkPos, glm::vec3(raftPos.x, sharkPos.y, raftPos.z));

                if (shark->damageFlashTimer > 0.0f) {
                    shark->damageFlashTimer -= deltaTime;
                }

                // Force shark animation to static bind pose (-1) and state to CIRCLING for now per user request
                shark->state = SharkState::CIRCLING;
                shark->currentAnimIndex = 0; // -1 to fully disable all skeletal movement

                // Temporary logic: since the user wants it static on the ship first, we don't 
                // modify the transform to circle around. Later, we can add circling logic here.
            }
        }
    };

}
