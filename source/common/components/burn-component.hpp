#pragma once

#include "ecs/component.hpp"

namespace our {

    class BurnComponent : public Component {
    public:
        float remainingTime = 5.0f;     // Seconds left of burning
        float damagePerSecond = 15.0f;   // DPS while burning
        float burnIntensity = 1.0f;      // 0..1 drives fire shader visual

        static std::string getID() { return "Burn"; }

        void deserialize(const nlohmann::json& data) override;
    };

}
