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
    class PlayerControllerSystem {
        Application* app; 
        bool mouse_locked = false;

    public:
        // Sets the application environment when traversing state
        void enter(Application* app){
            this->app = app;
        }

        void update(World* world, float deltaTime) {
            PlayerControllerComponent* player = nullptr;
            Entity* playerEntity = nullptr;
            
            // Search for the entity that has the PlayerControllerComponent
            for(auto entity : world->getEntities()){
                player = entity->getComponent<PlayerControllerComponent>();
                if(player) {
                    playerEntity = entity;
                    break;
                }
            }

            if(!player || !playerEntity) return;

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

            // Apply jumping with Mock Gravity System
            if(player->isGrounded && app->getKeyboard().justPressed(GLFW_KEY_SPACE)){
                player->yVelocity = player->jumpVelocity;
                player->isGrounded = false;
            }

            if(!player->isGrounded) {
                // gravity pull
                player->yVelocity -= player->gravity * deltaTime;
            }
            playerPos.y += player->yVelocity * deltaTime;

            // Boundary clamping constraint on Y axis (Walk onto the raft!)
            if(playerPos.y <= player->surfaceHeight){
                playerPos.y = player->surfaceHeight;
                player->yVelocity = 0.0f;
                player->isGrounded = true;
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