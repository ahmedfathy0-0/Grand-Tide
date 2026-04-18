#pragma once

#include "../ecs/component.hpp"
#include <glm/glm.hpp>

namespace our {

    enum class LightType {
        DIRECTIONAL,
        POINT
    };

    class LightComponent : public Component {
    public:
        glm::vec3 color = glm::vec3(1.0f);
        float intensity = 1.0f;
        LightType type = LightType::DIRECTIONAL;

        static std::string getID() { return "Light"; }

        void deserialize(const nlohmann::json& data) override;
    };

}