#include "enemy.hpp"

namespace our
{
    void EnemyComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        std::string typeStr = data.value("enemyType", "shark");

        if (typeStr == "musket")
            type = EnemyType::MUSKET;
        else if (typeStr == "marine_boat")
            type = EnemyType::MARINE_BOAT;
        else if (typeStr == "marine_ship")
            type = EnemyType::MARINE_SHIP;
        else if (typeStr == "spawner")
            type = EnemyType::SPAWNER;
        else
            type = EnemyType::SHARK;

        attackAnimationIndex = data.value("attackAnimationIndex", attackAnimationIndex);

        attackCooldown = data.value("attackCooldown", attackCooldown);
        attackDamage = data.value("attackDamage", attackDamage);

        std::string targetStr = data.value("primaryTarget", "RAFT");
        if (targetStr == "PLAYER")
            primaryTarget = PrimaryTarget::PLAYER;
        else
            primaryTarget = PrimaryTarget::RAFT;
    }
}