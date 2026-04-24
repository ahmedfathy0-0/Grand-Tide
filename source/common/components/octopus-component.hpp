#pragma once

#include "ecs/component.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our {

    enum class OctopusState {
        IDLE = 0,
        ANGRY = 1,
        DEAD = 2,
        TESTING = 3
    };

    class OctopusComponent : public Component {
    public:
        OctopusState state = OctopusState::TESTING;
        float health = 500.0f;
        float currentAnimationTime = 0.0f;
        int currentAnimIndex = 0;
        
        float stateTimer = 0.0f;
        float speed = 2.0f;
        
        // This is the animated model id or name loaded via ModelLoader
        std::string modelName; 
        
        // This stores the computed bone matrices for this frame
        std::vector<glm::mat4> finalBonesMatrices;

        // The ID of this component type is "Octopus"
        static std::string getID() { return "Octopus"; }
        
        // Reads from json
        void deserialize(const nlohmann::json& data) override;
    };

}
