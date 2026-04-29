#pragma once

#include "ecs/component.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our {

    enum class FireballState {
        IDLE = 0,       // Hands visible, no fire
        AIMING = 1,     // Right-click held, fire in hands, AOE indicator shown
        THROWING = 2    // Throw animation playing, fireball launched
    };

    class FireballComponent : public Component {
    public:
        FireballState state = FireballState::IDLE;

        // Animation data for the throw-arms model
        float currentAnimationTime = 0.0f;
        int currentAnimIndex = 0;
        std::string modelName = "throw_arms";
        std::vector<glm::mat4> finalBonesMatrices;

        // Fireball properties
        float throwSpeed = 30.0f;
        float aoeRadius = 5.0f;
        float burnDuration = 2.0f;
        float burnDamagePerSecond = 15.0f;
        float cooldown = 1.0f;
        float cooldownTimer = 0.0f;

        // Throw animation timing (seconds)
        float throwAnimDuration = 0.6f;
        float throwAnimTimer = 0.0f;

        // AOE indicator
        glm::vec3 aimTarget = glm::vec3(0.0f);
        bool hasAimTarget = false;

        static std::string getID() { return "Fireball"; }

        void deserialize(const nlohmann::json& data) override;
    };

}
