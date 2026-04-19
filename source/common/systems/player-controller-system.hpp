#pragma once

#include "../ecs/world.hpp"
#include "../components/camera.hpp"
#include "../components/player-controller.hpp"
#include "../application.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/fast_trigonometry.hpp>

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
        const int iters = 3; // Reduced from 20 to 3 so the raft only tilts on the massive rolling waves, ignoring chaotic tiny ripples
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

    public:
        // Sets the application environment when traversing state
        void enter(Application* app){
            this->app = app;
        }

        void update(World* world, float deltaTime) {
            PlayerControllerComponent* player = nullptr;
            Entity* playerEntity = nullptr;
            Entity* raftEntity = nullptr;
            
            // Search for the entity that has the PlayerControllerComponent and the raft
            for(auto entity : world->getEntities()){
                if(entity->name == "raft") {
                    raftEntity = entity;
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

            // X/Z Keyboard Movement updates linearly
            if(app->getKeyboard().isPressed(GLFW_KEY_W)) playerPos += front * ds;
            if(app->getKeyboard().isPressed(GLFW_KEY_S)) playerPos -= front * ds;
            if(app->getKeyboard().isPressed(GLFW_KEY_D)) playerPos += right * ds;
            if(app->getKeyboard().isPressed(GLFW_KEY_A)) playerPos -= right * ds;

            // Buffered jump logic makes jumping tolerant to frame/input timing jitter.
            if(jumpBufferTimer > 0.0f && coyoteTimer > 0.0f){
                player->yVelocity = player->jumpVelocity;
                player->isGrounded = false;
                jumpBufferTimer = 0.0f;
                coyoteTimer = 0.0f;
            }

            if(!player->isGrounded) {
                // gravity pull
                player->yVelocity -= player->gravity * deltaTime;
            }
            playerPos.y += player->yVelocity * deltaTime;

            // Check if player is directly above the raft surface bounds
            bool overRaft = (std::abs(playerPos.x - player->surfaceCenter.x) <= player->surfaceExtents.x) &&
                            (std::abs(playerPos.z - player->surfaceCenter.y) <= player->surfaceExtents.y);

            // Boundary clamping constraint on Y axis (Walk onto the raft!)
            if(overRaft && playerPos.y <= dynamicSurfaceHeight){
                playerPos.y = dynamicSurfaceHeight;
                player->yVelocity = 0.0f;
                player->isGrounded = true;
            } else if (!overRaft && playerPos.y <= waveHeight - 1.5f) { // Fall into water and submerge
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