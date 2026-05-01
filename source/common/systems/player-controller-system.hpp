#pragma once

#include "../ecs/world.hpp"
#include "../components/camera.hpp"
#include "../components/player-controller.hpp"
#include "../components/health.hpp"
#include "../application.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/fast_trigonometry.hpp>
#include <iostream>

namespace our
{
    inline float getWaveHeight(float posX, float posZ, float time) {
        const int ITERS_TRACE = 9;
        const float SCRL_SPEED = 0.4f;
        const glm::vec2 scrollDir = {1.0f, 1.0f};
        const float HOR_SCALE = 1.1f;
        const float FREQ = 0.6f;
        const float OCC_SPEED = 0.4f;
        const float WAV_ROT = 1.21f;
        const float FREQ_SCL = 1.2f;
        const float TIME_SCL = 1.095f;
        const float DRAG = 0.9f;
        const float WEIGHT_SCL = 0.8f;
        const float HEIGHT_DIV = 2.5f;

        float wav = 0.0f;
        glm::vec2 wavDir = {1.0f, 0.0f};
        float wavWeight = 1.0f;
        
        glm::vec2 wavPos = {posX, posZ};
        wavPos += time * SCRL_SPEED * scrollDir;
        wavPos *= HOR_SCALE;
        
        float wavFreq = FREQ;
        float wavTime = OCC_SPEED * time;
        
        auto rot = [](glm::vec2 v, float a) {
            float c = std::cos(a);
            float s = std::sin(a);
            return glm::vec2(v.x * c - v.y * s, v.x * s + v.y * c);
        };

        for(int i = 0; i < ITERS_TRACE; i++) {
            wavDir = rot(wavDir, WAV_ROT);
            float x = glm::dot(wavDir, wavPos) * wavFreq + wavTime;
            float wave = std::exp(std::sin(x) - 1.0f) * wavWeight;
            wav += wave;
            wavFreq *= FREQ_SCL;
            wavTime *= TIME_SCL;
            
            wavPos -= wavDir * wave * DRAG * std::cos(x);
            wavWeight *= WEIGHT_SCL;
        }
        
        float wavSum = -(std::pow(WEIGHT_SCL, float(ITERS_TRACE)) - 1.0f) * HEIGHT_DIV;
        return wav / wavSum;
    }

    inline glm::vec2 getWaveDx(float posX, float posZ, float time) {
        const int iters = 3; 
        const float SCRL_SPEED = 0.4f;
        const glm::vec2 scrollDir = {1.0f, 1.0f};
        const float HOR_SCALE = 1.1f;
        const float FREQ = 0.6f;
        const float OCC_SPEED = 0.4f;
        const float WAV_ROT = 1.21f;
        const float FREQ_SCL = 1.2f;
        const float TIME_SCL = 1.095f;
        const float DRAG = 0.9f;
        const float WEIGHT_SCL = 0.8f;
        const float HEIGHT_DIV = 2.5f;
        const float DX_DET = 0.65f;

        glm::vec2 dx = {0.0f, 0.0f};
        glm::vec2 wavDir = {1.0f, 0.0f};
        float wavWeight = 1.0f;
        
        glm::vec2 wavPos = {posX, posZ};
        wavPos += time * SCRL_SPEED * scrollDir;
        wavPos *= HOR_SCALE;
        
        float wavFreq = FREQ;
        float wavTime = OCC_SPEED * time;
        
        auto rot = [](glm::vec2 v, float a) {
            float c = std::cos(a);
            float s = std::sin(a);
            return glm::vec2(v.x * c - v.y * s, v.x * s + v.y * c);
        };

        for(int i = 0; i < iters; i++) {
            wavDir = rot(wavDir, WAV_ROT);
            float x = glm::dot(wavDir, wavPos) * wavFreq + wavTime;
            float result = std::exp(std::sin(x) - 1.0f) * std::cos(x);
            result *= wavWeight;
            dx += result * wavDir / std::pow(wavWeight, DX_DET);
            wavFreq *= FREQ_SCL;
            wavTime *= TIME_SCL;
            wavPos -= wavDir * result * DRAG;
            wavWeight *= WEIGHT_SCL;
        }
        
        float wavSum = -(std::pow(WEIGHT_SCL, float(iters)) - 1.0f) * HEIGHT_DIV;
        return dx / std::pow(wavSum, 1.0f - DX_DET);
    }

    class PlayerControllerSystem {
        Application* app; 
        bool mouse_locked = false;
        float jumpBufferTimer = 0.0f;
        float coyoteTimer = 0.0f;

        static constexpr float JumpBufferDuration = 0.12f;
        static constexpr float CoyoteDuration = 0.10f;
        bool isDrivingBoat = false; // Tracks if the player has taken control of the boat

    public:
        // Sets the application environment when traversing state
        void enter(Application* app){
            this->app = app;
        }

        void update(World* world, float deltaTime) {
            PlayerControllerComponent* player = nullptr;
            Entity* playerEntity = nullptr;
            Entity* raftEntity = nullptr;
            Entity* boatEntity = nullptr;
            
            // Search for the entity that has the PlayerControllerComponent and the raft
            for(auto entity : world->getEntities()){
                if(entity->name == "raft") {
                    raftEntity = entity;
                }
                if(entity->name == "boat") {
                    boatEntity = entity;
                }
                auto p = entity->getComponent<PlayerControllerComponent>();
                if(p) {
                    player = p;
                    playerEntity = entity;
                }
            }

            if(!player || !playerEntity) return;

            // Animate raft position on waves
            float time = (float)glfwGetTime();
            float waveHeight = 0.0f;
            float dynamicSurfaceHeight = 0.0f;
            if(raftEntity) {
                // Simulate a gentle, stable floating ship/raft without tying it to the complex shader waves
                float bobbingY = std::sin(time * 1.5f) * 0.05f; // Small vertical bobbing
                float swayX = std::sin(time * 1.0f) * 0.02f;    // Gentle pitch sway
                float swayZ = std::cos(time * 1.2f) * 0.02f;    // Gentle roll sway
                
                raftEntity->localTransform.position.y = 0.0f + bobbingY; // Raised base height to 0.0f + bobbing
                raftEntity->localTransform.rotation.x = swayX;
                raftEntity->localTransform.rotation.z = swayZ;

                // Set dynamicSurfaceHeight so the player feet roughly match the raft
                dynamicSurfaceHeight = raftEntity->localTransform.position.y + player->surfaceHeight;
                waveHeight = raftEntity->localTransform.position.y;
            }

            float dynamicBoatSurfaceHeight = 0.0f;
            if(boatEntity) {
                float bX = boatEntity->localTransform.position.x;
                float bZ = boatEntity->localTransform.position.z;
                
                // 1. REALISTIC HEIGHT (Bobbing) using the engine's wave function 
                // We add an offset (e.g. -0.8f) so that the boat sits deeper in the water and portions of it clip slightly under the waves.
                float realisticHeight = getWaveHeight(bX, bZ, time) - 0.8f;
                boatEntity->localTransform.position.y = realisticHeight;

                // 2. REALISTIC TILTING (Pitch & Roll) using the wave slope derivative
                glm::vec2 slope = getWaveDx(bX, bZ, time);
                boatEntity->localTransform.rotation.x = glm::radians(-90.0f) + (std::atan(slope.y) * 0.25f);  
                boatEntity->localTransform.rotation.z = -std::atan(slope.x) * 0.25f; // Roll
                
                dynamicBoatSurfaceHeight = realisticHeight + player->surfaceHeight; // Approximate foot height on boat
            }

            // Find the referenced camera entity
            Entity* cameraEntity = nullptr;
            if(!player->cameraEntityName.empty()) {
                for(auto entity : world->getEntities()){
                    if(entity->name == player->cameraEntityName) {
                        cameraEntity = entity;
                        break;
                    }
                }
            }

            // Mouse handling for rotation locking
            if(!mouse_locked){
                app->getMouse().lockMouse(app->getWindow());
                mouse_locked = true;
            }

            glm::vec3& playerPos = playerEntity->localTransform.position;
            glm::vec3& playerRot = playerEntity->localTransform.rotation;

            if(player->isGrounded) {
                coyoteTimer = CoyoteDuration;
            } else {
                coyoteTimer = glm::max(0.0f, coyoteTimer - deltaTime);
            }

            if(app->getKeyboard().justPressed(GLFW_KEY_SPACE)) {
                jumpBufferTimer = JumpBufferDuration;
            } else {
                jumpBufferTimer = glm::max(0.0f, jumpBufferTimer - deltaTime);
            }
            
            glm::vec2 delta = app->getMouse().getMouseDelta();
            
            if (!isDrivingBoat) {
                // Yaw applies to the player body (left/right)
                playerRot.y -= delta.x * player->rotationSensitivity; 
                    
                // Pitch applies to the camera (up/down)
                // If there's a separate camera entity referenced, pitch that. Otherwise, pitch the player itself.
                if(cameraEntity) {
                    glm::vec3& camRot = cameraEntity->localTransform.rotation;
                    camRot.x -= delta.y * player->rotationSensitivity;

                    // Clamping pitch for camera to prevent tumbling logic
                    if(camRot.x < -glm::half_pi<float>() * 0.99f) camRot.x = -glm::half_pi<float>() * 0.99f;
                    if(camRot.x >  glm::half_pi<float>() * 0.99f) camRot.x  = glm::half_pi<float>() * 0.99f;
                } else {
                    playerRot.x -= delta.y * player->rotationSensitivity;

                    if(playerRot.x < -glm::half_pi<float>() * 0.99f) playerRot.x = -glm::half_pi<float>() * 0.99f;
                    if(playerRot.x >  glm::half_pi<float>() * 0.99f) playerRot.x  = glm::half_pi<float>() * 0.99f;
                }

                playerRot.y = glm::wrapAngle(playerRot.y);
            }

            // Compute the X/Z oriented front and right vectors so that player only moves horizontally
            glm::mat4 matrix = playerEntity->localTransform.toMat4();
            glm::vec3 front = glm::normalize(glm::vec3(matrix * glm::vec4(0, 0, -1, 0)));
            glm::vec3 right = glm::normalize(glm::vec3(matrix * glm::vec4(1, 0, 0, 0)));

            // Force pure horizontal constraints onto X and Z.
            front.y = 0; 
            if (glm::length(front) > 0.001f) front = glm::normalize(front);
            
            right.y = 0; 
            if (glm::length(right) > 0.001f) right = glm::normalize(right);

            float current_speed = player->speed;
            if(app->getKeyboard().isPressed(GLFW_KEY_LEFT_SHIFT)) current_speed *= player->speedupFactor;

            float ds = current_speed * deltaTime;

            // Check if player is directly above the boat surface bounds for interaction (accounting for rotation)
            bool overBoat = false;
            if (boatEntity) {
                float dx = playerPos.x - boatEntity->localTransform.position.x;
                float dz = playerPos.z - boatEntity->localTransform.position.z;
                float yaw = boatEntity->localTransform.rotation.y;
                
                // Inverse rotate the offset to boat's local space
                float localX = dx * std::cos(-yaw) - dz * std::sin(-yaw);
                float localZ = dx * std::sin(-yaw) + dz * std::cos(-yaw);
                
                overBoat = (std::abs(localX) <= 6.5f) && (std::abs(localZ) <= 3.0f);
            }

            if (app->getKeyboard().justPressed(GLFW_KEY_F)) {
                if (isDrivingBoat) {
                    isDrivingBoat = false;
                    // Stop driving but remain at the center of the boat
                    if (boatEntity) {
                        playerPos.x = boatEntity->localTransform.position.x;
                        playerPos.z = boatEntity->localTransform.position.z;
                    }
                } else if (overBoat) {
                    isDrivingBoat = true;
                    // Center the camera pitch to look forward
                    if (cameraEntity) {
                        cameraEntity->localTransform.rotation.x = 0.0f;
                    } else {
                        playerRot.x = 0.0f;
                    }
                }
            }

            if (isDrivingBoat && boatEntity) {
                bool isMovingForward = app->getKeyboard().isPressed(GLFW_KEY_W);
                bool isMovingBackward = app->getKeyboard().isPressed(GLFW_KEY_S);

                // Move boat forward and backward instead of player
                if(isMovingForward) boatEntity->localTransform.position += front * ds * 1.5f;
                if(isMovingBackward) boatEntity->localTransform.position -= front * ds * 0.75f;

                // Steer the boat left and right (rotate) only when moving
                if (isMovingForward || isMovingBackward) {
                    float steer_speed = 0.8f * deltaTime;
                    float steer_dir = isMovingBackward && !isMovingForward ? -1.0f : 1.0f; // reverse turning when backing up

                    if(app->getKeyboard().isPressed(GLFW_KEY_A)) playerRot.y += steer_speed * steer_dir;
                    if(app->getKeyboard().isPressed(GLFW_KEY_D)) playerRot.y -= steer_speed * steer_dir;
                }

                // Precise OBB collision check with MTV (Minimum Translation Vector) to allow sliding
                if (raftEntity) {
                    float rX = raftEntity->localTransform.position.x;
                    float rZ = raftEntity->localTransform.position.z;
                    // Raft bounds
                    float rxExt = player->surfaceExtents.x;
                    float rzExt = player->surfaceExtents.y;
                    
                    float yaw = boatEntity->localTransform.rotation.y;
                    
                    glm::vec2 finalPush(0.0f);

                    // 1. Push boat out if its sample points are inside the raft AABB
                    auto toWorld = [&](float lx, float lz) {
                        return glm::vec2(
                            boatEntity->localTransform.position.x + lx * std::cos(yaw) - lz * std::sin(yaw),
                            boatEntity->localTransform.position.z + lx * std::sin(yaw) + lz * std::cos(yaw)
                        );
                    };

                    for(int i = -1; i <= 1; i++) {
                        for(int j = -1; j <= 1; j++) {
                            glm::vec2 pt = toWorld(i * 6.5f, j * 3.0f);
                            float dx = pt.x - rX;
                            float dz = pt.y - rZ; // pt.y is actually the Z coord here
                            
                            float penX = rxExt - std::abs(dx);
                            float penZ = rzExt - std::abs(dz);
                            
                            // Apply push if penetrating more than tolerance
                            if (penX > 0.05f && penZ > 0.05f) {
                                glm::vec2 push(0.0f);
                                if (penX < penZ) push.x = (dx > 0) ? penX : -penX;
                                else             push.y = (dz > 0) ? penZ : -penZ;
                                
                                if (std::abs(push.x) > std::abs(finalPush.x)) finalPush.x = push.x;
                                if (std::abs(push.y) > std::abs(finalPush.y)) finalPush.y = push.y;
                            }
                        }
                    }

                    // 2. Push boat out if raft corners are inside the boat OBB
                    auto getRaftCornerPush = [&](float wx, float wz) {
                        float dx = wx - boatEntity->localTransform.position.x;
                        float dz = wz - boatEntity->localTransform.position.z;
                        
                        float localX = dx * std::cos(-yaw) - dz * std::sin(-yaw);
                        float localZ = dx * std::sin(-yaw) + dz * std::cos(-yaw);
                        
                        float penX = 6.5f - std::abs(localX);
                        float penZ = 3.0f - std::abs(localZ);
                        
                        if (penX > 0.05f && penZ > 0.05f) {
                            glm::vec2 localPush(0.0f);
                            if (penX < penZ) localPush.x = (localX > 0) ? -penX : penX;
                            else             localPush.y = (localZ > 0) ? -penZ : penZ;
                            
                            return glm::vec2(
                                localPush.x * std::cos(yaw) - localPush.y * std::sin(yaw),
                                localPush.x * std::sin(yaw) + localPush.y * std::cos(yaw)
                            );
                        }
                        return glm::vec2(0.0f);
                    };

                    glm::vec2 corners[4] = {
                        getRaftCornerPush(rX + rxExt, rZ + rzExt),
                        getRaftCornerPush(rX + rxExt, rZ - rzExt),
                        getRaftCornerPush(rX - rxExt, rZ + rzExt),
                        getRaftCornerPush(rX - rxExt, rZ - rzExt)
                    };

                    for (const auto& cp : corners) {
                        if (std::abs(cp.x) > std::abs(finalPush.x)) finalPush.x = cp.x;
                        if (std::abs(cp.y) > std::abs(finalPush.y)) finalPush.y = cp.y;
                    }

                    // Apply the minimum translation vector to smoothly slide the boat out
                    if (std::abs(finalPush.x) > 0.0f || std::abs(finalPush.y) > 0.0f) {
                        boatEntity->localTransform.position.x += finalPush.x;
                        boatEntity->localTransform.position.z += finalPush.y;
                    }
                }
                
                // Align boat to where the player looks. 
                // The boat model is oriented sideways by roughly 90 degrees based on its initial pitch setup offset
                boatEntity->localTransform.rotation.y = playerRot.y + glm::radians(90.0f);
                
                // Lock player position to boat center
                playerPos.x = boatEntity->localTransform.position.x;
                playerPos.z = boatEntity->localTransform.position.z;
                player->yVelocity = 0.0f; // Prevent jumping
            } else {
                // X/Z Keyboard Movement updates linearly
                if(app->getKeyboard().isPressed(GLFW_KEY_W)) playerPos += front * ds;
                if(app->getKeyboard().isPressed(GLFW_KEY_S)) playerPos -= front * ds;
                if(app->getKeyboard().isPressed(GLFW_KEY_D)) playerPos += right * ds;
                if(app->getKeyboard().isPressed(GLFW_KEY_A)) playerPos -= right * ds;
            }

            // Apply jumping with Mock Gravity System
            if(player->isGrounded && app->getKeyboard().justPressed(GLFW_KEY_SPACE)){
                if (!isDrivingBoat) { // Can't jump while driving
                    player->yVelocity = player->jumpVelocity;
                    player->isGrounded = false;
                }
            }

            if(!player->isGrounded) {
                // gravity pull
                player->yVelocity -= player->gravity * deltaTime;
            }
            playerPos.y += player->yVelocity * deltaTime;

            // Check if player is directly above the raft surface bounds
            bool overRaft = (std::abs(playerPos.x - player->surfaceCenter.x) <= player->surfaceExtents.x) &&
                            (std::abs(playerPos.z - player->surfaceCenter.y) <= player->surfaceExtents.y);

            // Ensure overBoat is up-to-date with new player positions
            overBoat = false;
            if (boatEntity) {
                float dx = playerPos.x - boatEntity->localTransform.position.x;
                float dz = playerPos.z - boatEntity->localTransform.position.z;
                float yaw = boatEntity->localTransform.rotation.y;
                
                // Inverse rotate the offset to boat's local space
                float localX = dx * std::cos(-yaw) - dz * std::sin(-yaw);
                float localZ = dx * std::sin(-yaw) + dz * std::cos(-yaw);
                
                overBoat = (std::abs(localX) <= 6.5f) && (std::abs(localZ) <= 3.0f);
            }

            bool onStandableSurface = false;
            float targetSurfaceHeight = -100.0f;

            if(overRaft && playerPos.y <= dynamicSurfaceHeight + 0.1f) {
                targetSurfaceHeight = dynamicSurfaceHeight;
                onStandableSurface = true;
            } else if(overBoat && playerPos.y <= dynamicBoatSurfaceHeight + 0.1f) {
                targetSurfaceHeight = dynamicBoatSurfaceHeight;
                onStandableSurface = true;
            }

            // Boundary clamping constraint on Y axis (Walk onto the surfaces!)
            if(onStandableSurface && playerPos.y <= targetSurfaceHeight) {
                playerPos.y = targetSurfaceHeight;
                player->yVelocity = 0.0f;
                player->isGrounded = true;
            } else if (!overRaft && !overBoat && playerPos.y <= waveHeight - 1.5f) { // Fall into water and submerge
                // Lose 25 HP for falling in water
                auto* hp = playerEntity->getComponent<HealthComponent>();
                if (hp && hp->currentHealth > 0) {
                    hp->takeDamage(25.0f);
                    std::cout << "[Water] Player fell in water! -25 HP (now " << hp->currentHealth << ")" << std::endl;
                }
                // Reset position to center of the raft
                playerPos.x = player->surfaceCenter.x;
                playerPos.y = dynamicSurfaceHeight + 2.0f; // Drop them back from the sky slightly
                playerPos.z = player->surfaceCenter.y;
                player->yVelocity = 0.0f;
                player->isGrounded = false;
            } else {
                player->isGrounded = false;
            }
        }

        void exit(){
            if(mouse_locked) {
                mouse_locked = false;
                app->getMouse().unlockMouse(app->getWindow());
            }
        }
    };
}