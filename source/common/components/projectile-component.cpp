#include "projectile-component.hpp"

namespace our {

    void ProjectileComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;
        aoeRadius = data.value("aoeRadius", aoeRadius);
        burnDuration = data.value("burnDuration", burnDuration);
        burnDamagePerSecond = data.value("burnDamagePerSecond", burnDamagePerSecond);
        lifetime = data.value("lifetime", lifetime);
    }

}
