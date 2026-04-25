#include "octopus-component.hpp"
#include "deserialize-utils.hpp"

namespace our {

    void OctopusComponent::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;
        
        if (data.contains("state")) {
            std::string stateStr = data["state"].get<std::string>();
            if (stateStr == "IDLE") state = OctopusState::IDLE;
            else if (stateStr == "RISING") state = OctopusState::RISING;
            else if (stateStr == "SURFACED_IDLE") state = OctopusState::SURFACED_IDLE;
            else if (stateStr == "ATTACKING") state = OctopusState::ATTACKING;
            else if (stateStr == "DEAD") state = OctopusState::DEAD;
            else if (stateStr == "TESTING") state = OctopusState::TESTING;
        }
        health = data.value("health", health);
        currentAnimIndex = data.value("currentAnimIndex", currentAnimIndex);
        modelName = data.value("modelName", std::string(""));
        attackDamage = data.value("attackDamage", attackDamage);
        attackCooldown = data.value("attackCooldown", attackCooldown);
        attackDuration = data.value("attackDuration", attackDuration);
        idleDuration = data.value("idleDuration", idleDuration);
        emergeDelay = data.value("emergeDelay", emergeDelay);
        attackRange = data.value("attackRange", attackRange);
        surfacedY = data.value("surfacedY", surfacedY);
        submergedY = data.value("submergedY", submergedY);
        followDistance = data.value("followDistance", followDistance);
        minFollowDistance = data.value("minFollowDistance", minFollowDistance);
        maxFollowDistance = data.value("maxFollowDistance", maxFollowDistance);
        speed = data.value("speed", speed);
    }

}
