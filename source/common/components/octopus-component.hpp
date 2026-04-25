#pragma once

#include "ecs/component.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our {

    enum class OctopusState {
        IDLE = 0,          // Under water (submerged), waiting to emerge
        RISING = 1,        // Emerging from water with anim 12
        SURFACED_IDLE = 2, // On surface, idle, tracking player
        ATTACKING = 3,     // Attacking with anim 1
        DEAD = 4,
        TESTING = 5
    };

    class OctopusComponent : public Component {
    public:
        OctopusState state = OctopusState::IDLE;
        float health = 500.0f;
        float currentAnimationTime = 0.0f;
        int currentAnimIndex = 0;
        
        float stateTimer = 0.0f;
        float speed = 1.0f;
        
        // Attack properties
        float attackDamage = 50.0f;
        float attackCooldown = 2.0f;
        float attackTimer = 0.0f;
        float attackDuration = 30.0f;
        float idleDuration = 60.0f;
        float emergeDelay = 10.0f;
        float attackRange = 40.0f;           // Distance at which octopus can hit the player
        int attackCount = 0;                 // Number of attacks performed in this phase
        int consecutiveMisses = 0;           // Number of consecutive missed attacks
        bool useAttackAnim2 = false;         // Toggle between attack animation 1 and 2
        
        // Position properties
        float surfacedY = 1.0f;
        float submergedY = -150.0f;
        float minFollowDistance = 30.0f;
        float maxFollowDistance = 100.0f;
        float followDistance = 60.0f;
        
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
