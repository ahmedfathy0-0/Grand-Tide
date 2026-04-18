#include "player-controller.hpp"
#include "../deserialize-utils.hpp"

namespace our {
    void PlayerControllerComponent::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;
        speed = data.value("speed", speed);
        jumpVelocity = data.value("jumpVelocity", jumpVelocity);
        yVelocity = data.value("yVelocity", yVelocity);
        gravity = data.value("gravity", gravity);
        surfaceHeight = data.value("surfaceHeight", surfaceHeight);
        if(data.contains("surfaceCenter")) surfaceCenter = data.value("surfaceCenter", surfaceCenter);
        if(data.contains("surfaceExtents")) surfaceExtents = data.value("surfaceExtents", surfaceExtents);
        isGrounded = data.value("isGrounded", isGrounded);
        rotationSensitivity = data.value("rotationSensitivity", rotationSensitivity);
        speedupFactor = data.value("speedupFactor", speedupFactor);
        cameraEntityName = data.value("cameraEntityName", cameraEntityName);
    }
}