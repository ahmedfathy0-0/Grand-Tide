#pragma once

#include "ecs/world.hpp"
#include "components/octopus-component.hpp"
#include "components/health.hpp"
#include "ecs/transform.hpp"
#include "application.hpp"
#include "mesh/model.hpp"
#include <glm/glm.hpp>
#include <iostream>

namespace our {

    class OctopusSystem {
    public:
        void update(World* world, float deltaTime, Application* app) {
            // Find the raft entity
            Entity* raft = nullptr;
            for (auto entity : world->getEntities()) {
                if (entity->name == "raft") {
                    raft = entity;
                    break;
                }
            }

            if (!raft) return;

            // Update all octopuses
            for (auto entity : world->getEntities()) {
                auto octopus = entity->getComponent<OctopusComponent>();
                if (!octopus) continue;

                // Fixed offset from the raft so it mirrors movement (X and Z)
                glm::vec3 offset(40.0f, 0.0f, 40.0f); 
                glm::vec3& pos = entity->localTransform.position;
                pos.x = raft->localTransform.position.x + offset.x;
                pos.z = raft->localTransform.position.z + offset.z;

                // Determine target Y position based on state
                float targetY = -150.0f; // Submerged by default

                switch (octopus->state) {
                    case OctopusState::IDLE: { // Submerged state
                        octopus->currentAnimIndex = 0;
                        targetY = -150.0f;
                        octopus->stateTimer += deltaTime;
                        if (octopus->stateTimer > 10.0f) {
                            octopus->state = OctopusState::ANGRY;
                            octopus->stateTimer = 0.0f;
                        }
                        break;
                    }
                    case OctopusState::ANGRY: { // Surfaced state
                        octopus->currentAnimIndex = 1;
                        targetY = -12.0f; // Half submerged
                        octopus->stateTimer += deltaTime;
                        if (octopus->stateTimer > 5.0f) { // Stay above water for 5 seconds
                            octopus->state = OctopusState::IDLE;
                            octopus->stateTimer = 0.0f;
                        }
                        break;
                    }
                    case OctopusState::DEAD: {
                        world->markForRemoval(entity);
                        break;
                    }
                    case OctopusState::TESTING: {
                        targetY = -12.0f; // Always surfaced
                        octopus->stateTimer = 0.0f; // Never wet
                        
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

                // Log state once a second
                static float logTimer = 0.0f;
                logTimer += deltaTime;
                if (logTimer > 1.0f) {
                    std::cout << "Octopus State: " << (int)octopus->state << ", targetY: " << targetY << ", pos.y: " << pos.y << std::endl;
                    // Also print the number of animations available
                    auto it = ModelLoader::models.find(octopus->modelName);
                    if (it != ModelLoader::models.end()) {
                        Model* model = it->second;
                        if (model && model->getScene()) {
                            std::cout << "Num Animations in FBX: " << model->getScene()->mNumAnimations << std::endl;
                        }
                    }
                    logTimer = 0.0f;
                }

                // Smoothly interpolate Y position to rise and fall
                pos.y += (targetY - pos.y) * 2.0f * deltaTime;
            }
        }
    };

}
