#include "animation-system.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include "components/shark-component.hpp"
#include "components/octopus-component.hpp"

namespace our
{
    const aiNodeAnim *findNodeAnim(const aiAnimation *animation, const std::string &nodeName)
    {
        for (unsigned int i = 0; i < animation->mNumChannels; i++)
        {
            const aiNodeAnim *nodeAnim = animation->mChannels[i];
            if (std::string(nodeAnim->mNodeName.data) == nodeName)
            {
                return nodeAnim;
            }
        }
        return nullptr;
    }

    void AnimationSystem::update(World *world, float deltaTime)
    {
        for (auto entity : world->getEntities())
        {
            auto animator = entity->getComponent<AnimatorComponent>();
            if (animator && !animator->modelName.empty())
            {
                Model *model = ModelLoader::models[animator->modelName];
                if (model && model->getScene() && model->getScene()->HasAnimations())
                {
                    int animIndex = animator->currentAnimIndex;
                    if (animIndex >= (int)model->getScene()->mNumAnimations)
                    {
                        animIndex = 0;
                    }

                    if (animIndex >= 0)
                    {
                        aiAnimation *animation = model->getScene()->mAnimations[animIndex];
                        float ticksPerSecond = animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 25.0f;
                        float timeInTicks = deltaTime * ticksPerSecond * animator->playSpeed;

                        animator->currentAnimationTime += timeInTicks;
                        animator->currentAnimationTime = fmod(animator->currentAnimationTime, animation->mDuration);
                    }

                    animator->finalBonesMatrices.assign(model->getBoneCount(), glm::mat4(1.0f));
                    calculateBoneTransform(model->getScene()->mRootNode, model->getScene(), animator->currentAnimationTime, animIndex, glm::mat4(1.0f), model, animator);
                }
                else if (model && model->getBoneCount() > 0)
                {
                    animator->finalBonesMatrices.assign(model->getBoneCount(), glm::mat4(1.0f));
                }

                // Copy bone matrices to component-specific arrays for the renderer
                if (auto shark = entity->getComponent<SharkComponent>(); shark) {
                    shark->finalBonesMatrices = animator->finalBonesMatrices;
                }
                if (auto octopus = entity->getComponent<OctopusComponent>(); octopus) {
                    octopus->finalBonesMatrices = animator->finalBonesMatrices;
                }
            }
        }
    }

    void AnimationSystem::calculateBoneTransform(const aiNode *node, const aiScene *scene, float animationTime, int animIndex, const glm::mat4 &parentTransform, Model *model, AnimatorComponent *animator)
    {
        std::string nodeName(node->mName.data);
        glm::mat4 nodeTransform = AssimpMatToGlm(node->mTransformation);

        if (animIndex >= 0 && animIndex < scene->mNumAnimations)
        {
            aiAnimation *animation = scene->mAnimations[animIndex];
            const aiNodeAnim *nodeAnim = findNodeAnim(animation, nodeName);

            if (nodeAnim)
            {
                glm::vec3 scaling = calcInterpolatedScale(animationTime, nodeAnim);
                glm::mat4 scaleM = glm::scale(glm::mat4(1.0f), scaling);

                glm::quat rotation = calcInterpolatedRotation(animationTime, nodeAnim);
                glm::mat4 rotM = glm::mat4_cast(rotation);

                glm::vec3 translation = calcInterpolatedPosition(animationTime, nodeAnim);
                glm::mat4 transM = glm::translate(glm::mat4(1.0f), translation);

                nodeTransform = transM * rotM * scaleM;
            }
        }

        glm::mat4 globalTransform = parentTransform * nodeTransform;

        if (model->getBoneInfoMap().find(nodeName) != model->getBoneInfoMap().end())
        {
            int boneIndex = model->getBoneInfoMap()[nodeName].id;
            glm::mat4 offset = model->getBoneInfoMap()[nodeName].offset;
            glm::mat4 globalInverseTransform = glm::inverse(AssimpMatToGlm(scene->mRootNode->mTransformation));
            animator->finalBonesMatrices[boneIndex] = globalInverseTransform * globalTransform * offset;
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            calculateBoneTransform(node->mChildren[i], scene, animationTime, animIndex, globalTransform, model, animator);
        }
    }

    int AnimationSystem::getPositionIndex(float animationTime, const aiNodeAnim *nodeAnim)
    {
        for (unsigned int i = 0; i < nodeAnim->mNumPositionKeys - 1; i++)
        {
            if (animationTime < (float)nodeAnim->mPositionKeys[i + 1].mTime)
                return i;
        }
        return 0;
    }

    glm::vec3 AnimationSystem::calcInterpolatedPosition(float animationTime, const aiNodeAnim *nodeAnim)
    {
        if (nodeAnim->mNumPositionKeys == 1)
        {
            auto pos = nodeAnim->mPositionKeys[0].mValue;
            return glm::vec3(pos.x, pos.y, pos.z);
        }

        int p0Index = getPositionIndex(animationTime, nodeAnim);
        int p1Index = p0Index + 1;
        float scaleFactor = (animationTime - (float)nodeAnim->mPositionKeys[p0Index].mTime) /
                            (float)(nodeAnim->mPositionKeys[p1Index].mTime - nodeAnim->mPositionKeys[p0Index].mTime);
        auto p0 = nodeAnim->mPositionKeys[p0Index].mValue;
        auto p1 = nodeAnim->mPositionKeys[p1Index].mValue;

        glm::vec3 start = glm::vec3(p0.x, p0.y, p0.z);
        glm::vec3 end = glm::vec3(p1.x, p1.y, p1.z);
        return glm::mix(start, end, scaleFactor);
    }

    int AnimationSystem::getRotationIndex(float animationTime, const aiNodeAnim *nodeAnim)
    {
        for (unsigned int i = 0; i < nodeAnim->mNumRotationKeys - 1; i++)
        {
            if (animationTime < (float)nodeAnim->mRotationKeys[i + 1].mTime)
                return i;
        }
        return 0;
    }

    glm::quat AnimationSystem::calcInterpolatedRotation(float animationTime, const aiNodeAnim *nodeAnim)
    {
        if (nodeAnim->mNumRotationKeys == 1)
        {
            auto rot = nodeAnim->mRotationKeys[0].mValue;
            return glm::quat(rot.w, rot.x, rot.y, rot.z);
        }

        int p0Index = getRotationIndex(animationTime, nodeAnim);
        int p1Index = p0Index + 1;
        float scaleFactor = (animationTime - (float)nodeAnim->mRotationKeys[p0Index].mTime) /
                            (float)(nodeAnim->mRotationKeys[p1Index].mTime - nodeAnim->mRotationKeys[p0Index].mTime);
        auto p0 = nodeAnim->mRotationKeys[p0Index].mValue;
        auto p1 = nodeAnim->mRotationKeys[p1Index].mValue;

        glm::quat start = glm::quat(p0.w, p0.x, p0.y, p0.z);
        glm::quat end = glm::quat(p1.w, p1.x, p1.y, p1.z);
        return glm::slerp(start, end, scaleFactor);
    }

    int AnimationSystem::getScaleIndex(float animationTime, const aiNodeAnim *nodeAnim)
    {
        for (unsigned int i = 0; i < nodeAnim->mNumScalingKeys - 1; i++)
        {
            if (animationTime < (float)nodeAnim->mScalingKeys[i + 1].mTime)
                return i;
        }
        return 0;
    }

    glm::vec3 AnimationSystem::calcInterpolatedScale(float animationTime, const aiNodeAnim *nodeAnim)
    {
        if (nodeAnim->mNumScalingKeys == 1)
        {
            auto scale = nodeAnim->mScalingKeys[0].mValue;
            return glm::vec3(scale.x, scale.y, scale.z);
        }

        int p0Index = getScaleIndex(animationTime, nodeAnim);
        int p1Index = p0Index + 1;
        float scaleFactor = (animationTime - (float)nodeAnim->mScalingKeys[p0Index].mTime) /
                            (float)(nodeAnim->mScalingKeys[p1Index].mTime - nodeAnim->mScalingKeys[p0Index].mTime);
        auto p0 = nodeAnim->mScalingKeys[p0Index].mValue;
        auto p1 = nodeAnim->mScalingKeys[p1Index].mValue;

        glm::vec3 start = glm::vec3(p0.x, p0.y, p0.z);
        glm::vec3 end = glm::vec3(p1.x, p1.y, p1.z);
        return glm::mix(start, end, scaleFactor);
    }
}