#include "animation-system.hpp"
#include <glm/gtx/matrix_decompose.hpp>

namespace our {

    const aiNodeAnim* findNodeAnim(const aiAnimation* animation, const std::string& nodeName) {
        for (unsigned int i = 0; i < animation->mNumChannels; i++) {
            const aiNodeAnim* nodeAnim = animation->mChannels[i];
            if (std::string(nodeAnim->mNodeName.data) == nodeName) {
                return nodeAnim;
            }
        }
        return nullptr;
    }

    void AnimationSystem::update(World* world, float deltaTime) {
        for (auto entity : world->getEntities()) {
            auto shark = entity->getComponent<SharkComponent>();
            if (shark && !shark->modelName.empty()) {
                Model* model = ModelLoader::models[shark->modelName];
                if(model && model->getScene() && model->getScene()->HasAnimations()) {
                    int animIndex = shark->currentAnimIndex;
                    if (animIndex >= (int)model->getScene()->mNumAnimations) {
                        animIndex = 0;
                    }
                    
                    if (animIndex >= 0) {
                        aiAnimation* animation = model->getScene()->mAnimations[animIndex];
                        float ticksPerSecond = animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 25.0f;
                        float timeInTicks = deltaTime * ticksPerSecond;
                        if (animIndex == 4) {
                            timeInTicks *= 3.0f; // swim animation a little faster
                        }
                        shark->currentAnimationTime += timeInTicks;
                        shark->currentAnimationTime = fmod(shark->currentAnimationTime, animation->mDuration);
                    }

                    // Resize the output matrices for the shader
                    shark->finalBonesMatrices.assign(model->getBoneCount(), glm::mat4(1.0f));

                    calculateBoneTransform(model->getScene()->mRootNode, model->getScene(), shark->currentAnimationTime, animIndex, glm::mat4(1.0f), model, shark);
                }
            }
        }
    }

    void AnimationSystem::calculateBoneTransform(const aiNode* node, const aiScene* scene, float animationTime, int animIndex, const glm::mat4& parentTransform, Model* model, SharkComponent* shark) {
        std::string nodeName(node->mName.data);
        glm::mat4 nodeTransform = AssimpMatToGlm(node->mTransformation);

        // Only evaluate animation tracks if animIndex is valid
        if (animIndex >= 0 && animIndex < scene->mNumAnimations) {
            aiAnimation* animation = scene->mAnimations[animIndex];
            const aiNodeAnim* nodeAnim = findNodeAnim(animation, nodeName);
            
            if (nodeAnim) {
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
        
        if (model->getBoneInfoMap().find(nodeName) != model->getBoneInfoMap().end()) {
            int boneIndex = model->getBoneInfoMap()[nodeName].id;
            glm::mat4 offset = model->getBoneInfoMap()[nodeName].offset;
            // The Assimp root has a global inverse transform that we need if rotations are wrong,
            // but for simplicity, we apply globalTransform directly.
            glm::mat4 globalInverseTransform = glm::inverse(AssimpMatToGlm(scene->mRootNode->mTransformation));
            shark->finalBonesMatrices[boneIndex] = globalInverseTransform * globalTransform * offset;
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            calculateBoneTransform(node->mChildren[i], scene, animationTime, animIndex, globalTransform, model, shark);
        }
    }

    // Positions
    int AnimationSystem::getPositionIndex(float animationTime, const aiNodeAnim* nodeAnim) {
        for (unsigned int i = 0; i < nodeAnim->mNumPositionKeys - 1; i++) {
            if (animationTime < (float)nodeAnim->mPositionKeys[i + 1].mTime)
                return i;
        }
        return 0;
    }

    glm::vec3 AnimationSystem::calcInterpolatedPosition(float animationTime, const aiNodeAnim* nodeAnim) {
        if (nodeAnim->mNumPositionKeys == 1) {
            auto pos = nodeAnim->mPositionKeys[0].mValue;
            return glm::vec3(pos.x, pos.y, pos.z);
        }

        int p0Index = getPositionIndex(animationTime, nodeAnim);
        int p1Index = p0Index + 1;
        float scaleFactor = (animationTime - (float)nodeAnim->mPositionKeys[p0Index].mTime) / 
                            ((float)nodeAnim->mPositionKeys[p1Index].mTime - (float)nodeAnim->mPositionKeys[p0Index].mTime);
                            
        auto start = nodeAnim->mPositionKeys[p0Index].mValue;
        auto end = nodeAnim->mPositionKeys[p1Index].mValue;
        glm::vec3 st(start.x, start.y, start.z);
        glm::vec3 en(end.x, end.y, end.z);
        
        return glm::mix(st, en, scaleFactor);
    }
    
    // Rotations
    int AnimationSystem::getRotationIndex(float animationTime, const aiNodeAnim* nodeAnim) {
        for (unsigned int i = 0; i < nodeAnim->mNumRotationKeys - 1; i++) {
            if (animationTime < (float)nodeAnim->mRotationKeys[i + 1].mTime)
                return i;
        }
        return 0;
    }

    glm::quat AnimationSystem::calcInterpolatedRotation(float animationTime, const aiNodeAnim* nodeAnim) {
        if (nodeAnim->mNumRotationKeys == 1) {
            auto rot = nodeAnim->mRotationKeys[0].mValue;
            return glm::quat(rot.w, rot.x, rot.y, rot.z);
        }

        int p0Index = getRotationIndex(animationTime, nodeAnim);
        int p1Index = p0Index + 1;
        float scaleFactor = (animationTime - (float)nodeAnim->mRotationKeys[p0Index].mTime) / 
                            ((float)nodeAnim->mRotationKeys[p1Index].mTime - (float)nodeAnim->mRotationKeys[p0Index].mTime);
                            
        auto start = nodeAnim->mRotationKeys[p0Index].mValue;
        auto end = nodeAnim->mRotationKeys[p1Index].mValue;
        glm::quat st(start.w, start.x, start.y, start.z);
        glm::quat en(end.w, end.x, end.y, end.z);
        
        return glm::slerp(st, en, scaleFactor);
    }

    // Scales
    int AnimationSystem::getScaleIndex(float animationTime, const aiNodeAnim* nodeAnim) {
        for (unsigned int i = 0; i < nodeAnim->mNumScalingKeys - 1; i++) {
            if (animationTime < (float)nodeAnim->mScalingKeys[i + 1].mTime)
                return i;
        }
        return 0;
    }

    glm::vec3 AnimationSystem::calcInterpolatedScale(float animationTime, const aiNodeAnim* nodeAnim) {
        if (nodeAnim->mNumScalingKeys == 1) {
            auto scale = nodeAnim->mScalingKeys[0].mValue;
            return glm::vec3(scale.x, scale.y, scale.z);
        }

        int p0Index = getScaleIndex(animationTime, nodeAnim);
        int p1Index = p0Index + 1;
        float scaleFactor = (animationTime - (float)nodeAnim->mScalingKeys[p0Index].mTime) / 
                            ((float)nodeAnim->mScalingKeys[p1Index].mTime - (float)nodeAnim->mScalingKeys[p0Index].mTime);
                            
        auto start = nodeAnim->mScalingKeys[p0Index].mValue;
        auto end = nodeAnim->mScalingKeys[p1Index].mValue;
        glm::vec3 st(start.x, start.y, start.z);
        glm::vec3 en(end.x, end.y, end.z);
        
        return glm::mix(st, en, scaleFactor);
    }

}
