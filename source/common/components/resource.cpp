#include "resource.hpp"

namespace our {

    void ResourceComponent::deserialize(const nlohmann::json& data) {
        if (!data.is_object()) return;
        // To avoid conflicts with component "type", we might use "resourceType".
        // But if the JSON gives {"type": "wood"} inside a "Resource" object, we handle it.
        if (data.contains("Resource")) {
            auto res = data["Resource"];
            type = res.value("type", type);
            amount = res.value("amount", amount);
        } else {
            type = data.value("resourceType", data.value("type", type));
            amount = data.value("amount", amount);
        }
    }
}
