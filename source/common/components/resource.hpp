#pragma once

#include "../ecs/component.hpp"
#include <string>

namespace our {

    class ResourceComponent : public Component {
    public:
        std::string type; // "wood" or "fish"
        int amount = 1;
        float lifetime = 60.0f; // seconds until auto-despawn
        float elapsed = 0.0f;   // accumulated time

        static std::string getID() { return "Resource"; }

        void deserialize(const nlohmann::json& data) override;
    };

}
