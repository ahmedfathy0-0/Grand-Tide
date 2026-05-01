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
        float approachDistance = 30.0f; // Distance at which boat stops and muskets can fire
        MARINE_BOAT_STATE state = MARINE_BOAT_STATE::IDLE;

        static std::string getID() { return "Marine Boat"; }

        void deserialize(const nlohmann::json &data) override
        {
            if (!data.is_object()) return;
            modelName = data.value("modelName", modelName);
            speed = data.value("speed", speed);
            attackRange = data.value("attackRange", attackRange);
            approachDistance = data.value("approachDistance", approachDistance);
        }
    };
}