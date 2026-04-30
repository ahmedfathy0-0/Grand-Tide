#pragma once

#include "ecs/component.hpp"
#include <string>

namespace our
{

    enum class EnemyType
    {
        SHARK = 0,
        MUSKET = 1,
        MARINE_SHIP = 2,
        MARINE_BOAT = 3,
        SPAWNER = 4,
    };

    enum class PrimaryTarget
    {
        RAFT = 0,
        PLAYER = 1
    };

    enum class EnemyState
    {
        ALIVE = 0,
        DEAD = 1
    };

    class EnemyComponent : public Component
    {
    public:
        EnemyType type;
        PrimaryTarget primaryTarget;
        EnemyState state = EnemyState::ALIVE;
        float attackDamage;
        float attackCooldown = 0.0f;
        int attackAnimationIndex;

        static std::string getID() { return "Enemy"; }

        void deserialize(const nlohmann::json &data) override;
    };

}