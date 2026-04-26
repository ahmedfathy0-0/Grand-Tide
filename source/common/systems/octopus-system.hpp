#pragma once

#include "ecs/world.hpp"
#include "components/octopus-component.hpp"
#include "components/health.hpp"
#include "components/animator.hpp"
#include "ecs/transform.hpp"
#include "mesh/model.hpp"
#include "application.hpp"
#include <glm/glm.hpp>
#include <iostream>

namespace our {

    inline float getAnimDurationSeconds(Model* model, int animIndex) {
        if (!model || !model->getScene() || !model->getScene()->HasAnimations()) return 1.0f;
        if (animIndex < 0 || animIndex >= (int)model->getScene()->mNumAnimations) return 1.0f;
        aiAnimation* anim = model->getScene()->mAnimations[animIndex];
        float tps = anim->mTicksPerSecond != 0.0f ? (float)anim->mTicksPerSecond : 25.0f;
        return (float)anim->mDuration / tps;
    }

    class OctopusSystem {
    public:
        void update(World* world, float deltaTime, Application* app) {
            Entity* raft = nullptr;
            Entity* player = nullptr;
            for (auto entity : world->getEntities()) {
                if (entity->name == "raft") raft = entity;
                if (entity->name == "player") player = entity;
            }

            if (!raft) return;

            for (auto entity : world->getEntities()) {
                auto octopus = entity->getComponent<OctopusComponent>();
                if (!octopus) continue;

                auto animator = entity->getComponent<AnimatorComponent>();
                glm::vec3& pos = entity->localTransform.position;
                float targetY = octopus->submergedY;

                // --- STEP 1: HIDDEN (underwater, waiting for spawnDelay) ---
                if (octopus->state == OctopusState::HIDDEN) {
                    if (!octopus->initialized) {
                        pos = raft->localTransform.position;
                        pos.x += octopus->spawnDistance;
                        pos.y = octopus->submergedY;
                        octopus->initialized = true;
                    }
                    octopus->currentAnimIndex = OctopusAnimation::IDLE;
                    targetY = octopus->submergedY;
                    octopus->stateTimer += deltaTime;
                    if (octopus->stateTimer >= octopus->spawnDelay) {
                        octopus->state = OctopusState::SURFACING;
                        octopus->stateTimer = 0; octopus->animElapsedTime = 0;
                        octopus->currentAnimIndex = OctopusAnimation::SKILL_SPAWN;
                        if (animator) {
                            animator->currentAnimIndex = (int)OctopusAnimation::SKILL_SPAWN;
                            animator->currentAnimationTime = 0; animator->isLooping = false;
                        }
                        if (animator && !animator->modelName.empty()) {
                            Model* m = ModelLoader::models[animator->modelName];
                            octopus->currentAnimDuration = getAnimDurationSeconds(m, (int)OctopusAnimation::SKILL_SPAWN);
                        }
                    }
                }
                // --- STEP 2: SURFACING (rise Y + play spawn anim once) ---
                else if (octopus->state == OctopusState::SURFACING) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::SKILL_SPAWN;
                    octopus->stateTimer += deltaTime;
                    octopus->animElapsedTime += deltaTime;
                    float p = glm::clamp(octopus->stateTimer / octopus->surfaceDuration, 0.0f, 1.0f);
                    pos.y = octopus->submergedY + (octopus->surfacedY - octopus->submergedY) * p;
                    if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                        octopus->state = OctopusState::WAIT_BEFORE_FIGHT;
                        octopus->stateTimer = 0;
                    }
                }
                // --- STEP 3: WAIT_BEFORE_FIGHT (freeze on last spawn frame) ---
                else if (octopus->state == OctopusState::WAIT_BEFORE_FIGHT) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::SKILL_SPAWN;
                    if (animator) animator->isLooping = false;
                    octopus->stateTimer += deltaTime;
                    if (octopus->stateTimer >= octopus->waitBeforeFightDuration) {
                        octopus->state = OctopusState::FIGHT_START;
                        octopus->stateTimer = 0; octopus->animElapsedTime = 0;
                        octopus->currentAnimIndex = OctopusAnimation::SKILL_DANCE_START;
                        if (animator) {
                            animator->currentAnimIndex = (int)OctopusAnimation::SKILL_DANCE_START;
                            animator->currentAnimationTime = 0; animator->isLooping = false;
                        }
                        if (animator && !animator->modelName.empty()) {
                            Model* m = ModelLoader::models[animator->modelName];
                            octopus->currentAnimDuration = getAnimDurationSeconds(m, (int)OctopusAnimation::SKILL_DANCE_START);
                        }
                    }
                }
                // --- STEP 4: FIGHT_START (play skill01 once) ---
                else if (octopus->state == OctopusState::FIGHT_START) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::SKILL_DANCE_START;
                    octopus->animElapsedTime += deltaTime;
                    if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                        octopus->state = OctopusState::COMBAT_IDLE;
                        octopus->stateTimer = 0; octopus->animElapsedTime = 0;
                        octopus->currentAnimIndex = OctopusAnimation::COMBAT_IDLE;
                        octopus->bossReady = true;
                        if (animator) {
                            animator->currentAnimIndex = (int)OctopusAnimation::COMBAT_IDLE;
                            animator->currentAnimationTime = 0; animator->isLooping = true;
                        }
                    }
                }
                // --- STEP 5: COMBAT_IDLE (looping, emit boss_ready) ---
                else if (octopus->state == OctopusState::COMBAT_IDLE) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::COMBAT_IDLE;
                }
                else if (octopus->state == OctopusState::ATTACKING) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::ATTACK;
                }
                else if (octopus->state == OctopusState::MOVING) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::RUN_FORWARD;
                }
                else if (octopus->state == OctopusState::ENRAGED) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::SKILL_DANCE_HALF_HEALTH;
                }
                else if (octopus->state == OctopusState::DYING) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::DEATH;
                }

                if (octopus->state != OctopusState::SURFACING) {
                    pos.y += (targetY - pos.y) * 2.0f * deltaTime;
                }
                if (player) {
                    glm::vec3 diff = player->localTransform.position - pos;
                    diff.y = 0.0f;
                    if (glm::length(diff) > 0.01f) entity->localTransform.rotation.y = atan2(diff.x, diff.z);
                }

                if (animator && animator->currentAnimIndex != (int)octopus->currentAnimIndex) {
                    animator->currentAnimIndex = (int)octopus->currentAnimIndex;
                    animator->currentAnimationTime = 0.0f;
                }
            }
        }
    };
}