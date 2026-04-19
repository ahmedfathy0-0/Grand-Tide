#pragma once

#include "ecs/component.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our {

    enum class SharkState {
        APPROACHING = 0,
        ATTACKING = 1,
        SUBMERGED = 2,
        DEAD = 3
    };

    class SharkComponent : public Component {
    public:
        SharkState state = SharkState::APPROACHING;
        float health = 100.0f;
        float currentAnimationTime = 0.0f;
        int currentAnimIndex = 0; // 0 for Swim, 1 for Bite for example
        float damageFlashTimer = 0.0f; // > 0 means flash red
        
        float stateTimer = 0.0f;
        float speed = 7.0f;
        
        // This is the animated model id or name loaded via ModelLoader
        std::string modelName; 
        
        // This stores the computed bone matrices for this frame
        std::vector<glm::mat4> finalBonesMatrices;

        // The ID of this component type is "Shark"
        static std::string getID() { return "Shark"; }
        
        // Reads from json
        void deserialize(const nlohmann::json& data) override;
    };

}
