#pragma once

#include "ecs/component.hpp"
#include <glm/glm.hpp>

namespace our {

    class ProjectileComponent : public Component {
    public:
        glm::vec3 velocity = glm::vec3(0.0f);
        float aoeRadius = 5.0f;
        float burnDuration = 5.0f;
        float burnDamagePerSecond = 15.0f;
        float lifetime = 5.0f;    // Max seconds before auto-detonation
        float age = 0.0f;
        bool active = true;

        static std::string getID() { return "Projectile"; }

        void deserialize(const nlohmann::json& data) override;
    };

}
