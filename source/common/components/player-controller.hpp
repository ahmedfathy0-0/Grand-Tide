#pragma once

#include "../ecs/component.hpp"
#include <glm/glm.hpp>
#include <string>

namespace our {

    // This component denotes that the PlayerControllerSystem will move the owning entity using user inputs.
    // It allows proper character control with constrained walking on a surface and jumping behavior.
    class PlayerControllerComponent : public Component {
    public:
        float speed = 5.0f;
        float jumpVelocity = 5.0f;
        float yVelocity = 0.0f; 
        float gravity = 15.0f;
        float surfaceHeight = 0.0f; // Height of the raft to walk on
        bool isGrounded = true;
        
        float rotationSensitivity = 0.01f;
        float speedupFactor = 2.0f;
        
        std::string cameraEntityName = "";

        // The ID of this component type is "PlayerController"
        static std::string getID() { return "PlayerController"; }

        // Reads variables from the given json object
        void deserialize(const nlohmann::json& data) override;
    };

}