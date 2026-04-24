#include "shark-component.hpp"
#include "deserialize-utils.hpp"

namespace our {

    void SharkComponent::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;
        
        if (data.contains("state")) {
            std::string stateStr = data["state"].get<std::string>();
            if (stateStr == "APPROACHING") state = SharkState::APPROACHING;
            else if (stateStr == "ATTACKING") state = SharkState::ATTACKING;
            else if (stateStr == "SUBMERGED") state = SharkState::SUBMERGED;
            else if (stateStr == "DEAD") state = SharkState::DEAD;
        }
        health = data.value("health", health);
        speed = data.value("speed", speed);
        attackRange = data.value("attackRange", attackRange);
    }

}
