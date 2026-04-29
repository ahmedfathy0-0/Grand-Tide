#pragma once

#include "ecs/component.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our
{
    class AnimatorComponent : public Component
    {
    public:
        std::string modelName; // FBX model key in ModelLoader::models
        float currentAnimationTime = 0.0f;
        int currentAnimIndex = 0;
        float playSpeed = 1.0f;
        bool isLooping = true; // If false, animation clamps at end (one-shot)
        bool loopAnimation = true; // Set to false for death animations to hold last frame
        std::vector<glm::mat4> finalBonesMatrices; // Computed bone matrices per frame

        static std::string getID() { return "Animator"; }

        void deserialize(const nlohmann::json &data) override;
    };
}