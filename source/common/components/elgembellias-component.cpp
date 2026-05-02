#include "elgembellias-component.hpp"
#include "deserialize-utils.hpp"

namespace our {

    void ElgembelliasComponent::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;

        if (data.contains("state")) {
            std::string stateStr = data["state"].get<std::string>();
            if (stateStr == "HIDDEN") state = ElgembelliasState::HIDDEN;
            else if (stateStr == "SURFACING") state = ElgembelliasState::SURFACING;
            else if (stateStr == "DANCING") state = ElgembelliasState::DANCING;
            else if (stateStr == "CHASE") state = ElgembelliasState::CHASE;
            else if (stateStr == "ATTACKING") state = ElgembelliasState::ATTACKING;
            else if (stateStr == "DANCE_2") state = ElgembelliasState::DANCE_2;
            else if (stateStr == "MISS_DANCE") state = ElgembelliasState::MISS_DANCE;
            else if (stateStr == "POST_HIT") state = ElgembelliasState::POST_HIT;
            else if (stateStr == "DAMAGE") state = ElgembelliasState::DAMAGE;
            else if (stateStr == "DEATH") state = ElgembelliasState::DEATH;
        }
        surfacedY = data.value("surfacedY", surfacedY);
        submergedY = data.value("submergedY", submergedY);
        surfaceSpeed = data.value("surfaceSpeed", surfaceSpeed);
        spawnDistance = data.value("spawnDistance", spawnDistance);
        minDistFromOctopus = data.value("minDistFromOctopus", minDistFromOctopus);
        danceDuration = data.value("danceDuration", danceDuration);
        kickDuration = data.value("kickDuration", kickDuration);
        attackRange = data.value("attackRange", attackRange);
        chaseSpeed = data.value("chaseSpeed", chaseSpeed);
        kickDamage = data.value("kickDamage", kickDamage);
        kickHitRadius = data.value("kickHitRadius", kickHitRadius);
        maxHits = data.value("maxHits", maxHits);
        dance2Duration = data.value("dance2Duration", dance2Duration);
        damageDuration = data.value("damageDuration", damageDuration);
        missDanceDuration = data.value("missDanceDuration", missDanceDuration);
        postHitDanceDuration = data.value("postHitDanceDuration", postHitDanceDuration);
        minDistance = data.value("minDistance", minDistance);
        maxHealth = data.value("maxHealth", maxHealth);
        currentHealth = data.value("currentHealth", maxHealth);
    }

}
