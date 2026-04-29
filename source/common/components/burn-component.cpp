#include "burn-component.hpp"

namespace our {

    void BurnComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;
        remainingTime = data.value("remainingTime", remainingTime);
        damagePerSecond = data.value("damagePerSecond", damagePerSecond);
        burnIntensity = data.value("burnIntensity", burnIntensity);
    }

}
