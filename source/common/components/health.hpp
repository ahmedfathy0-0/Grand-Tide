#pragma once

#include "../ecs/component.hpp"
#include <json/json.hpp>

namespace our {

    class HealthComponent : public Component {
    public:
        float maxHealth = 100.0f;
        float currentHealth = 100.0f;
        float damageFlashTimer = 0.0f;  // Timer for screen blood effect when damaged
        bool justDamaged = false;        // Set to true each time takeDamage() is called

        static std::string getID() { return "Health"; }

        void takeDamage(float amount);
        void heal(float amount);
        bool isDead() const;

        void deserialize(const nlohmann::json& data) override;
    };

}
