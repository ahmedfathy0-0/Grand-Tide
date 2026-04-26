#include "animator.hpp"

namespace our
{
    void AnimatorComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;
        modelName = data.value("modelName", "");
        currentAnimIndex = data.value("currentAnimIndex", 0);
        playSpeed = data.value("playSpeed", 1.0f);
        isLooping = data.value("isLooping", true);
    }
}