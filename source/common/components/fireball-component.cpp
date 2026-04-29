#include "fireball-component.hpp"
#include "../deserialize-utils.hpp"

namespace our {

    void FireballComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;
        throwSpeed = data.value("throwSpeed", throwSpeed);
        aoeRadius = data.value("aoeRadius", aoeRadius);
        burnDuration = data.value("burnDuration", burnDuration);
        burnDamagePerSecond = data.value("burnDamagePerSecond", burnDamagePerSecond);
        cooldown = data.value("cooldown", cooldown);
        throwAnimDuration = data.value("throwAnimDuration", throwAnimDuration);
        if (data.contains("modelName"))
            modelName = data["modelName"].get<std::string>();
    }

}
