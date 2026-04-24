#include "octopus-component.hpp"
#include "deserialize-utils.hpp"

namespace our {

    void OctopusComponent::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;
        
        if (data.contains("state")) {
            std::string stateStr = data["state"].get<std::string>();
            if (stateStr == "IDLE") state = OctopusState::IDLE;
            else if (stateStr == "ANGRY") state = OctopusState::ANGRY;
            else if (stateStr == "DEAD") state = OctopusState::DEAD;
            else if (stateStr == "TESTING") state = OctopusState::TESTING;
        }
        health = data.value("health", health);
        currentAnimIndex = data.value("currentAnimIndex", currentAnimIndex);
        modelName = data.value("modelName", std::string(""));
    }

}
