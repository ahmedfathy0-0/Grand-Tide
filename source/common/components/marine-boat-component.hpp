#pragma once

#include "ecs/component.hpp"
#include <string>
#include <glm/glm.hpp>

namespace our

{
    enum class MARINE_BOAT_STATE
    {
        IDLE = 0,
        APPROACHING = 1
    };
    class MarineBoatComponent : public Component
    {
    public:
        std::string modelName = "marine_boat";
        float speed = 8.0f;
        float attackRange = 40.0f;
        MARINE_BOAT_STATE state = MARINE_BOAT_STATE::IDLE;

        static std::string getID() { return "Marine Boat"; }

        void deserialize(const nlohmann::json &data) override
        {
            if (data.contains("modelName"))
                modelName = data["modelName"].get<std::string>();
            if (data.contains("speed"))
                speed = data["speed"].get<float>();
            if (data.contains("attackRange"))
                attackRange = data["attackRange"].get<float>();
        }
    };
}