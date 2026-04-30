#pragma once
#include "ecs/world.hpp"
#include "mesh/model.hpp"
#include "components/animator.hpp"

namespace our
{

    class AnimationSystem
    {
    public:
        // Update all animations based on delta time
        void update(World *world, float deltaTime);

        // Pre-compute bone matrices for rendering
        void calculateBoneTransform(const aiNode *node, const aiScene *scene, float animationTime, int animIndex, const glm::mat4 &parentTransform, Model *model, AnimatorComponent *animator);

        // Find best keyframes
        int getPositionIndex(float animationTime, const aiNodeAnim *nodeAnim);
        int getRotationIndex(float animationTime, const aiNodeAnim *nodeAnim);
        int getScaleIndex(float animationTime, const aiNodeAnim *nodeAnim);

        // Interpolations
        glm::vec3 calcInterpolatedPosition(float animationTime, const aiNodeAnim *nodeAnim);
        glm::quat calcInterpolatedRotation(float animationTime, const aiNodeAnim *nodeAnim);
        glm::vec3 calcInterpolatedScale(float animationTime, const aiNodeAnim *nodeAnim);
    };

}