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
     
        float damageFlashTimer = 0.0f; // > 0 means flash red
        bool hasAttackedBefore = false; // First attack uses 15°, subsequent use 8°
        
        float stateTimer = 0.0f;
        float speed = 12.0;
        float attackRange = 25.0f;
        
        // Bone matrices computed by AnimationSystem, copied here for renderer
        std::vector<glm::mat4> finalBonesMatrices;
        
        // This is the animated model id or name loaded via ModelLoader

        // The ID of this component type is "Shark"
        static std::string getID() { return "Shark"; }
        
        // Reads from json
        void deserialize(const nlohmann::json& data) override;
    };

}
