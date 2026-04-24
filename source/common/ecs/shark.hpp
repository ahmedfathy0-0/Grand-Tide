#pragma once

#include "entity.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our
{

    enum class SharkState
    {
        APPROACHING = 0,
        ATTACKING = 1,
        SUBMERGED = 2,
        DEAD = 3
    };

    class Shark : public Entity
    {
    public:
        SharkState state = SharkState::APPROACHING;
        float damageFlashTimer = 0.0f; // > 0 means flash red
        float stateTimer = 0.0f;
        float speed = 7.0f;
        float attackRange = 25.0f;

        // This is the animated model id or name loaded via ModelLoader

        // The ID of this component type is "Shark"
        static std::string getID() { return "Shark"; }
    };

}
