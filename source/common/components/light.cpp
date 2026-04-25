#include "light.hpp"
#include "../deserialize-utils.hpp"
#include <iostream>

namespace our {
    void LightComponent::deserialize(const nlohmann::json& data) {
        if(!data.is_object()) return;
        std::string type_str = data.value("lightType", "directional");
        if (type_str == "point") type = LightType::POINT;
        else if (type_str == "spot") type = LightType::SPOT;
        else type = LightType::DIRECTIONAL;

        color = data.value("color", color);
        intensity = data.value("intensity", intensity);
        if(data.contains("direction")) direction = data["direction"];
        if(data.contains("cone_angles")) {
            cone_angles.x = glm::radians(data["cone_angles"][0].get<float>());
            cone_angles.y = glm::radians(data["cone_angles"][1].get<float>());
        }
    }
}