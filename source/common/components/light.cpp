#include "light.hpp"
#include "../deserialize-utils.hpp"
#include <iostream>

namespace our {
    void LightComponent::deserialize(const nlohmann::json& data) {
        if(!data.is_object()) return;
        std::string type_str = data.value("lightType", "directional");
        if (type_str == "point") type = LightType::POINT;
        else type = LightType::DIRECTIONAL;

        color = data.value("color", color);
        intensity = data.value("intensity", intensity);
    }
}