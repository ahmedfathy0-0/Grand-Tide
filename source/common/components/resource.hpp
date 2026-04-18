#pragma once

#include "../ecs/component.hpp"
#include <string>

namespace our {

    class ResourceComponent : public Component {
    public:
        std::string type; // "wood" or "fish"
        int amount = 1;

        static std::string getID() { return "Resource"; }

        void deserialize(const nlohmann::json& data) override;
    };

}
