#pragma once

#include "ecs/component.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our {

    enum class ElgembelliasState {
        HIDDEN = 0,       // Underwater, invisible, waiting to appear
        SURFACING = 1,    // Rising to the surface
        DANCING = 2,      // Performing dance animation, facing player
        KICKING = 3,      // Performing kick animation
    };

    class ElgembelliasComponent : public Component {
    public:
        ElgembelliasState state = ElgembelliasState::HIDDEN;

        float stateTimer = 0.0f;
        float surfacedY = -5.0f;       // Y position when surfaced (at water level)
        float submergedY = -150.0f;    // Y position when hidden underwater
        float surfaceSpeed = 5.0f;     // Speed of surfacing animation

        // Animation
        int danceAnimIndex = -1;       // Will be discovered at runtime
        int kickAnimIndex = -1;        // Will be discovered at runtime
        int currentAnimIndex = 0;
        bool loopAnimation = true;

        // Spawn configuration
        float spawnDistance = 120.0f;  // Distance from raft to spawn
        float minDistFromOctopus = 60.0f; // Min distance from octopus death position
        bool spawned = false;

        // Dance timing
        float danceTimer = 0.0f;
        float danceDuration = 15.0f;  // Duration of dance before kick
        float kickDuration = 5.0f;

        // Bone matrices computed by AnimationSystem
        std::vector<glm::mat4> finalBonesMatrices;

        // The ID of this component type is "Elgembellias"
        static std::string getID() { return "Elgembellias"; }

        // Reads from json
        void deserialize(const nlohmann::json& data) override;
    };

}
