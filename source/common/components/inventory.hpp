#pragma once

#include "../ecs/component.hpp"

namespace our {

    class InventoryComponent : public Component {
    public:
        int activeSlot = 1; // 1 = Hammer, 2 = Net, 3 = Spear
        int woodCount = 0;
        int fishCount = 0;

        static std::string getID() { return "Inventory"; }

        void deserialize(const nlohmann::json& data) override;
    };

}
