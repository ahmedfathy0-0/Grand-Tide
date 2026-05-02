#include "octopus-component.hpp"
#include "deserialize-utils.hpp"

namespace our {

    void OctopusComponent::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;
        
        if (data.contains("state")) {
            std::string stateStr = data["state"].get<std::string>();
            if (stateStr == "HIDDEN") state = OctopusState::HIDDEN;
            else if (stateStr == "SURFACING") state = OctopusState::SURFACING;
            else if (stateStr == "WAIT_BEFORE_FIGHT") state = OctopusState::WAIT_BEFORE_FIGHT;
            else if (stateStr == "FIGHT_START") state = OctopusState::FIGHT_START;
            else if (stateStr == "COMBAT_IDLE") state = OctopusState::COMBAT_IDLE;
            else if (stateStr == "ATTACKING") state = OctopusState::ATTACKING;
            else if (stateStr == "MOVING") state = OctopusState::MOVING;
            else if (stateStr == "ENRAGED") state = OctopusState::ENRAGED;
            else if (stateStr == "DYING") state = OctopusState::DYING;
            else if (stateStr == "DEATH_ANIM") state = OctopusState::DEATH_ANIM;
            else if (stateStr == "REVIVING") state = OctopusState::REVIVING;
        }
        health = data.value("health", health);
        currentAnimIndex = (OctopusAnimation)data.value("currentAnimIndex", (int)currentAnimIndex);
        modelName = data.value("modelName", std::string(""));
        attackDamage = data.value("attackDamage", attackDamage);
        attackCooldown = data.value("attackCooldown", attackCooldown);
        attackDuration = data.value("attackDuration", attackDuration);
        idleDuration = data.value("idleDuration", idleDuration);
        emergeDelay = data.value("emergeDelay", emergeDelay);
        attackRange = data.value("attackRange", attackRange);
        chaseSpeed = data.value("chaseSpeed", chaseSpeed);
        surfacedY = data.value("surfacedY", surfacedY);
        submergedY = data.value("submergedY", submergedY);
        followDistance = data.value("followDistance", followDistance);
        minFollowDistance = data.value("minFollowDistance", minFollowDistance);
        maxFollowDistance = data.value("maxFollowDistance", maxFollowDistance);
        speed = data.value("speed", speed);
        spawnDistance = data.value("spawnDistance", spawnDistance);
        spawnDelay = data.value("spawnDelay", spawnDelay);
        surfaceDuration = data.value("surfaceDuration", surfaceDuration);
        waitBeforeFightDuration = data.value("waitBeforeFightDuration", waitBeforeFightDuration);
        revivalHP = data.value("revivalHP", revivalHP);
        damageMultiplier = data.value("damageMultiplier", damageMultiplier);
        tentacleRadius = data.value("tentacleRadius", tentacleRadius);
        playerRadius = data.value("playerRadius", playerRadius);
        hitCooldownDuration = data.value("hitCooldownDuration", hitCooldownDuration);
        stunDuration = data.value("stunDuration", stunDuration);
        waveMaxRadius = data.value("waveMaxRadius", waveMaxRadius);
        waveExpandSpeed = data.value("waveExpandSpeed", waveExpandSpeed);
        waveDamage = data.value("waveDamage", waveDamage);
    }

}
