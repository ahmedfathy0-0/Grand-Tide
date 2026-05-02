#pragma once

#include "../ecs/component.hpp"
#include <glm/glm.hpp>

namespace our {

    enum class LightType {
        DIRECTIONAL,
        POINT,
        SPOT
    };

    class LightComponent : public Component {
    public:
        glm::vec3 color = glm::vec3(1.0f);
        float intensity = 1.0f;
        LightType type = LightType::DIRECTIONAL;

        // Spot light specific attributes
        glm::vec3 direction = glm::vec3(0, -1, 0);
        glm::vec2 cone_angles = glm::vec2(12.5f, 17.5f);

        static std::string getID() { return "Light"; }

        void deserialize(const nlohmann::json& data) override;
    };

}