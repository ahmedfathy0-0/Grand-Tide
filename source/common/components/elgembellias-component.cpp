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
            else if (stateStr == "KICKING") state = ElgembelliasState::KICKING;
        }
        surfacedY = data.value("surfacedY", surfacedY);
        submergedY = data.value("submergedY", submergedY);
        surfaceSpeed = data.value("surfaceSpeed", surfaceSpeed);
        spawnDistance = data.value("spawnDistance", spawnDistance);
        minDistFromOctopus = data.value("minDistFromOctopus", minDistFromOctopus);
        danceDuration = data.value("danceDuration", danceDuration);
        kickDuration = data.value("kickDuration", kickDuration);
    }

}
