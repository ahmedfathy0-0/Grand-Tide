#include "shark-component.hpp"
#include "deserialize-utils.hpp"

namespace our {

    void SharkComponent::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;
        
        if (data.contains("state")) {
            std::string stateStr = data["state"].get<std::string>();
            if (stateStr == "CIRCLING") state = SharkState::CIRCLING;
            else if (stateStr == "ATTACKING") state = SharkState::ATTACKING;
            else if (stateStr == "DEAD") state = SharkState::DEAD;
        }
        health = data.value("health", health);
        currentAnimIndex = data.value("currentAnimIndex", currentAnimIndex);
        modelName = data.value("modelName", "");
    }

}
