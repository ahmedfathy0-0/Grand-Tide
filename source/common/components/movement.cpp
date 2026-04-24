#include "movement.hpp"
#include "../ecs/entity.hpp"
#include "../deserialize-utils.hpp"

namespace our
{
    // Reads linearVelocity & angularVelocity from the given json object
    void MovementComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        if (data.contains("linearVelocity"))
        {
            linearVelocity = data["linearVelocity"].get<glm::vec3>();
        }
        if (data.contains("angularVelocity"))
        {
            glm::vec3 vel = data["angularVelocity"].get<glm::vec3>();
            angularVelocity = glm::radians(vel);
        }
    }
}