#include "health.hpp"
#include <algorithm>

namespace our {

    void HealthComponent::takeDamage(float amount) {
        if (amount <= 0.0f || currentHealth <= 0.0f) return;
        currentHealth -= amount;
        if (currentHealth < 0.0f) {
            currentHealth = 0.0f;
        }
        justDamaged = true;
    }

    void HealthComponent::heal(float amount) {
        currentHealth += amount;
        if (currentHealth > maxHealth) {
            currentHealth = maxHealth;
        }
    }

    bool HealthComponent::isDead() const {
        return currentHealth <= 0.0f;
    }

    void HealthComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;
        maxHealth = data.value("maxHealth", maxHealth);
        currentHealth = data.value("currentHealth", maxHealth);
    }
}
