#pragma once

#include <ecs/world.hpp>
#include <components/elgembellias-component.hpp>
#include <components/animator.hpp>
#include <components/mesh-renderer.hpp>
#include <mesh/model.hpp>
#include <asset-loader.hpp>
#include <iostream>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

namespace our {

    class ElgembelliasSystem {
    public:
        // Activates the Elgembellias entity (already deserialized from JSON) by repositioning
        // it away from the octopus death position and starting its surfacing/dance behavior.
        // Returns the activated entity, or nullptr if not found.
        our::Entity* activate(World* world, const glm::vec3& octopusDeathPos) {
            for (auto entity : world->getEntities()) {
                auto comp = entity->getComponent<ElgembelliasComponent>();
                if (!comp) continue;

                // Reposition away from octopus death position if too close
                float distToOctopus = glm::length(glm::vec2(
                    entity->localTransform.position.x - octopusDeathPos.x,
                    entity->localTransform.position.z - octopusDeathPos.z));
                if (distToOctopus < comp->minDistFromOctopus) {
                    float angle = static_cast<float>(rand() % 360) * glm::pi<float>() / 180.0f;
                    entity->localTransform.position.x = octopusDeathPos.x + cos(angle) * comp->spawnDistance;
                    entity->localTransform.position.z = octopusDeathPos.z + sin(angle) * comp->spawnDistance;
                }
                // Keep it submerged initially
                entity->localTransform.position.y = comp->submergedY;
                comp->state = ElgembelliasState::HIDDEN;
                comp->spawned = true;

                // Discover dance/kick animation indices from the model
                auto anim = entity->getComponent<AnimatorComponent>();
                Model* model = ModelLoader::models["elgembellias"];
                if (model && model->getScene() && model->getScene()->HasAnimations()) {
                    std::cout << "[Elgembellias] Model has " << model->getScene()->mNumAnimations << " animations:" << std::endl;
                    for (unsigned int i = 0; i < model->getScene()->mNumAnimations; i++) {
                        std::string animName = model->getScene()->mAnimations[i]->mName.C_Str();
                        std::cout << "  [" << i << "] " << animName << std::endl;
                        if (animName.find("Dance") != std::string::npos ||
                            animName.find("dance") != std::string::npos ||
                            animName.find("DANCE") != std::string::npos) {
                            comp->danceAnimIndex = (int)i;
                        }
                        if (animName.find("Kick") != std::string::npos ||
                            animName.find("kick") != std::string::npos ||
                            animName.find("KICK") != std::string::npos) {
                            comp->kickAnimIndex = (int)i;
                        }
                    }
                }
                // Start dance animation
                if (anim && comp->danceAnimIndex >= 0) {
                    anim->currentAnimIndex = comp->danceAnimIndex;
                    anim->loopAnimation = true;
                }

                std::cout << "[Elgembellias] Activated at ("
                          << entity->localTransform.position.x << ","
                          << entity->localTransform.position.y << ","
                          << entity->localTransform.position.z << ")" << std::endl;
                return entity;
            }
            return nullptr;
        }

        void update(World* world, float deltaTime, Entity* player) {
            for (auto entity : world->getEntities()) {
                auto comp = entity->getComponent<ElgembelliasComponent>();
                if (!comp) continue;

                auto anim = entity->getComponent<AnimatorComponent>();

                switch (comp->state) {
                    case ElgembelliasState::HIDDEN: {
                        // Start surfacing immediately
                        comp->state = ElgembelliasState::SURFACING;
                        comp->stateTimer = 0.0f;
                        std::cout << "[Elgembellias] Starting to surface" << std::endl;
                        break;
                    }
                    case ElgembelliasState::SURFACING: {
                        comp->stateTimer += deltaTime;
                        // Lerp Y from submerged to surfaced
                        float t = glm::min(comp->stateTimer / (glm::abs(comp->surfacedY - comp->submergedY) / comp->surfaceSpeed), 1.0f);
                        entity->localTransform.position.y = glm::mix(comp->submergedY, comp->surfacedY, t);

                        if (t >= 1.0f) {
                            entity->localTransform.position.y = comp->surfacedY;
                            comp->state = ElgembelliasState::DANCING;
                            comp->stateTimer = 0.0f;
                            comp->danceTimer = 0.0f;
                            // Start dance animation
                            if (anim && comp->danceAnimIndex >= 0) {
                                anim->currentAnimIndex = comp->danceAnimIndex;
                                anim->loopAnimation = true;
                            }
                            std::cout << "[Elgembellias] Surfaced! Starting dance" << std::endl;
                        }
                        break;
                    }
                    case ElgembelliasState::DANCING: {
                        comp->stateTimer += deltaTime;
                        comp->danceTimer += deltaTime;

                        // Face the player
                        if (player) {
                            glm::vec3 diff = player->localTransform.position - entity->localTransform.position;
                            diff.y = 0.0f;
                            if (glm::length(diff) > 0.01f) {
                                entity->localTransform.rotation.y = atan2(diff.x, diff.z);
                            }
                        }

                        // Keep dance animation playing
                        if (anim) {
                            anim->currentAnimIndex = comp->danceAnimIndex >= 0 ? comp->danceAnimIndex : 0;
                            anim->loopAnimation = true;
                        }

                        // After dance duration, switch to kick
                        if (comp->danceTimer >= comp->danceDuration) {
                            comp->state = ElgembelliasState::KICKING;
                            comp->stateTimer = 0.0f;
                            if (anim && comp->kickAnimIndex >= 0) {
                                anim->currentAnimIndex = comp->kickAnimIndex;
                                anim->loopAnimation = true;
                            }
                            std::cout << "[Elgembellias] Switching to kick" << std::endl;
                        }
                        break;
                    }
                    case ElgembelliasState::KICKING: {
                        comp->stateTimer += deltaTime;

                        // Face the player
                        if (player) {
                            glm::vec3 diff = player->localTransform.position - entity->localTransform.position;
                            diff.y = 0.0f;
                            if (glm::length(diff) > 0.01f) {
                                entity->localTransform.rotation.y = atan2(diff.x, diff.z);
                            }
                        }

                        // After kick duration, go back to dancing
                        if (comp->stateTimer >= comp->kickDuration) {
                            comp->state = ElgembelliasState::DANCING;
                            comp->stateTimer = 0.0f;
                            comp->danceTimer = 0.0f;
                            if (anim && comp->danceAnimIndex >= 0) {
                                anim->currentAnimIndex = comp->danceAnimIndex;
                                anim->loopAnimation = true;
                            }
                            std::cout << "[Elgembellias] Back to dance" << std::endl;
                        }
                        break;
                    }
                }
                break; // Only one Elgembellias entity
            }
        }
    };

}
