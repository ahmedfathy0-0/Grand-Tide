#include "inventory.hpp"

namespace our {

    void InventoryComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;
        activeSlot = data.value("activeSlot", activeSlot);
        woodCount = data.value("woodCount", woodCount);
        fishCount = data.value("fishCount", fishCount);
    }
}
