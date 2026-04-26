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
#include <unordered_map>

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

        // ------------------------------------------------------------------
        // setAnimation: maps a clip name to the model's animation index.
        // You may override this body later; the mapping below is a stub.
        // ------------------------------------------------------------------
        static void setAnimation(AnimatorComponent* animator, const std::string& clipName, bool loop) {
            if (!animator) return;

            static const std::unordered_map<std::string, int> clipMap = {
                {"Mon_PiratesKing_Attack01",    (int)OctopusAnimation::ATTACK},
                {"Mon_PiratesKing_Walk01",      (int)OctopusAnimation::RUN_FORWARD},
                {"Mon_PiratesKing_Turn_L01",    (int)OctopusAnimation::MOVE_LEFT},
                {"Mon_PiratesKing_Turn_R01",    (int)OctopusAnimation::MOVE_RIGHT},
                {"Mon_PiratesKing_Cidle01",     (int)OctopusAnimation::COMBAT_IDLE},
                {"Mon_PiratesKing_Idle01",      (int)OctopusAnimation::IDLE},
                {"Mon_PiratesKing_Skill_Spawn", (int)OctopusAnimation::SKILL_SPAWN},
                {"Mon_PiratesKing_Skill_Dance", (int)OctopusAnimation::SKILL_DANCE_START},
                {"Mon_PiratesKing_Skill_Dance_Half", (int)OctopusAnimation::SKILL_DANCE_HALF_HEALTH},
                {"Mon_PiratesKing_Death",       (int)OctopusAnimation::DEATH},
            };

            auto it = clipMap.find(clipName);
            if (it != clipMap.end()) {
                animator->currentAnimIndex = it->second;
            } else {
                // Fallback: try to parse as integer index
                try { animator->currentAnimIndex = std::stoi(clipName); }
                catch (...) { /* keep current */ }
            }
            animator->currentAnimationTime = 0.0f;
            animator->isLooping = loop;
        }

        // ------------------------------------------------------------------
        // queryAnimDuration: returns seconds for a given clip name
        // ------------------------------------------------------------------
        static float queryAnimDuration(AnimatorComponent* animator, const std::string& clipName) {
            if (!animator || animator->modelName.empty()) return 1.0f;

            static const std::unordered_map<std::string, int> clipMap = {
                {"Mon_PiratesKing_Attack01",    (int)OctopusAnimation::ATTACK},
                {"Mon_PiratesKing_Walk01",      (int)OctopusAnimation::RUN_FORWARD},
                {"Mon_PiratesKing_Turn_L01",    (int)OctopusAnimation::MOVE_LEFT},
                {"Mon_PiratesKing_Turn_R01",    (int)OctopusAnimation::MOVE_RIGHT},
                {"Mon_PiratesKing_Cidle01",     (int)OctopusAnimation::COMBAT_IDLE},
                {"Mon_PiratesKing_Idle01",      (int)OctopusAnimation::IDLE},
                {"Mon_PiratesKing_Skill_Spawn", (int)OctopusAnimation::SKILL_SPAWN},
                {"Mon_PiratesKing_Skill_Dance", (int)OctopusAnimation::SKILL_DANCE_START},
                {"Mon_PiratesKing_Skill_Dance_Half", (int)OctopusAnimation::SKILL_DANCE_HALF_HEALTH},
                {"Mon_PiratesKing_Death",       (int)OctopusAnimation::DEATH},
            };

            int idx = (int)OctopusAnimation::IDLE;
            auto it = clipMap.find(clipName);
            if (it != clipMap.end()) idx = it->second;
            else {
                try { idx = std::stoi(clipName); }
                catch (...) {}
            }

            Model* m = ModelLoader::models[animator->modelName];
            return getAnimDurationSeconds(m, idx);
        }

    private:

        // ------------------------------------------------------------------
        // Helper: is the player within range AND inside a 90-degree forward cone?
        // ------------------------------------------------------------------
        static bool isInAttackCone(const glm::vec3& bossPos, float bossRotY,
                                   const glm::vec3& playerPos, float range) {
            glm::vec3 toPlayer = playerPos - bossPos;
            toPlayer.y = 0.0f;
            float dist = glm::length(toPlayer);
            if (dist > range) return false;
            if (dist < 0.01f) return true; // right on top of player = definitely in cone

            glm::vec3 bossForward(std::sin(bossRotY), 0.0f, std::cos(bossRotY));
            glm::vec3 dirToPlayer = glm::normalize(toPlayer);

            float dot = glm::dot(bossForward, dirToPlayer);
            dot = glm::clamp(dot, -1.0f, 1.0f);
            float angleDeg = glm::degrees(std::acos(dot));

            return angleDeg <= 45.0f;           // 90-degree cone = +/- 45
        }

        // ------------------------------------------------------------------
        // Helper: signed angle from boss forward to player direction (degrees)
        // Negative = player is to the left, Positive = player is to the right
        // ------------------------------------------------------------------
        static float getSignedAngleToPlayer(const glm::vec3& bossPos, float bossRotY,
                                            const glm::vec3& playerPos) {
            glm::vec3 toPlayer = playerPos - bossPos;
            toPlayer.y = 0.0f;
            if (glm::length(toPlayer) < 0.01f) return 0.0f;

            glm::vec3 bossForward(std::sin(bossRotY), 0.0f, std::cos(bossRotY));
            glm::vec3 dirToPlayer = glm::normalize(toPlayer);

            float dot = glm::dot(bossForward, dirToPlayer);
            dot = glm::clamp(dot, -1.0f, 1.0f);
            float angleDeg = glm::degrees(std::acos(dot));

            // Sign from cross product Y component
            float crossY = bossForward.x * dirToPlayer.z - bossForward.z * dirToPlayer.x;
            if (crossY < 0.0f) angleDeg = -angleDeg;

            return angleDeg;
        }

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

                // Face the player at all times during combat states (before state logic)
                if (player) {
                    glm::vec3 diff = player->localTransform.position - pos;
                    diff.y = 0.0f;
                    if (glm::length(diff) > 0.01f) entity->localTransform.rotation.y = atan2(diff.x, diff.z);
                }

                // ==============================================================
                // STEP 1: HIDDEN (underwater, waiting for spawnDelay)
                // ==============================================================
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
                        setAnimation(animator, "Mon_PiratesKing_Skill_Spawn", false);
                        octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Skill_Spawn");
                    }
                }
                // ==============================================================
                // STEP 2: SURFACING (rise Y + play spawn anim once)
                // ==============================================================
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
                // ==============================================================
                // STEP 3: WAIT_BEFORE_FIGHT (freeze on last spawn frame)
                // ==============================================================
                else if (octopus->state == OctopusState::WAIT_BEFORE_FIGHT) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::SKILL_SPAWN;
                    if (animator) animator->isLooping = false;
                    octopus->stateTimer += deltaTime;
                    if (octopus->stateTimer >= octopus->waitBeforeFightDuration) {
                        octopus->state = OctopusState::FIGHT_START;
                        octopus->stateTimer = 0; octopus->animElapsedTime = 0;
                        octopus->currentAnimIndex = OctopusAnimation::SKILL_DANCE_START;
                        setAnimation(animator, "Mon_PiratesKing_Skill_Dance", false);
                        octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Skill_Dance");
                    }
                }
                // ==============================================================
                // STEP 4: FIGHT_START (play skill dance once)
                // ==============================================================
                else if (octopus->state == OctopusState::FIGHT_START) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::SKILL_DANCE_START;
                    octopus->animElapsedTime += deltaTime;
                    if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                        octopus->state = OctopusState::COMBAT_IDLE;
                        octopus->stateTimer = 0; octopus->animElapsedTime = 0;
                        octopus->currentAnimIndex = OctopusAnimation::COMBAT_IDLE;
                        octopus->bossReady = true;
                        setAnimation(animator, "Mon_PiratesKing_Cidle01", true);
                    }
                }
                // ==============================================================
                // STEP 5: COMBAT_IDLE (looping, boss_ready = true)
                // ==============================================================
                else if (octopus->state == OctopusState::COMBAT_IDLE) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::COMBAT_IDLE;

                    // Transition to ATTACKING when boss is ready and player is in range+cone
                    if (octopus->bossReady && player) {
                        glm::vec3 toPlayer = player->localTransform.position - pos;
                        toPlayer.y = 0.0f;
                        float dist = glm::length(toPlayer);

                        if (dist <= octopus->attackRange) {
                            float rotY = entity->localTransform.rotation.y;
                            if (isInAttackCone(pos, rotY, player->localTransform.position, octopus->attackRange)) {
                                // Player in range and cone -- start attacking
                                octopus->state = OctopusState::ATTACKING;
                                octopus->animElapsedTime = 0.0f;
                                octopus->consecutiveMisses = 0;
                                octopus->currentAnimIndex = OctopusAnimation::ATTACK;
                                setAnimation(animator, "Mon_PiratesKing_Attack01", false);
                                octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Attack01");
                            } else {
                                // In range but outside cone -- move toward player
                                octopus->state = OctopusState::MOVING;
                                octopus->animElapsedTime = 0.0f;
                                octopus->consecutiveMisses = 0;
                            }
                        } else {
                            // Out of range -- move toward player
                            octopus->state = OctopusState::MOVING;
                            octopus->animElapsedTime = 0.0f;
                            octopus->consecutiveMisses = 0;
                        }
                    }
                }
                // ==============================================================
                // STEP 6: ATTACKING (play attack once, check hit/miss)
                // ==============================================================
                else if (octopus->state == OctopusState::ATTACKING) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::ATTACK;
                    octopus->animElapsedTime += deltaTime;

                    // Wait for the attack animation to finish
                    if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                        // Attack animation done -- check if it hit the player
                        bool hit = false;
                        if (player) {
                            float rotY = entity->localTransform.rotation.y;
                            hit = isInAttackCone(pos, rotY, player->localTransform.position, octopus->attackRange);
                        }

                        if (hit) {
                            // --------------------------------------------------
                            // HIT: deal damage, reset miss count, start next attack cycle
                            // --------------------------------------------------
                            octopus->consecutiveMisses = 0;
                            octopus->attackCount++;
                            if (player) {
                                auto playerHealth = player->getComponent<HealthComponent>();
                                if (playerHealth) {
                                    playerHealth->takeDamage(octopus->attackDamage);
                                    std::cout << "[Octopus] Attack HIT! Dealt " << octopus->attackDamage
                                              << " damage. Total attacks: " << octopus->attackCount << std::endl;
                                }
                            }
                            // Restart attack cycle
                            octopus->animElapsedTime = 0.0f;
                            setAnimation(animator, "Mon_PiratesKing_Attack01", false);
                            octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Attack01");
                        } else {
                            // --------------------------------------------------
                            // MISS: increment miss count
                            // --------------------------------------------------
                            octopus->consecutiveMisses++;
                            std::cout << "[Octopus] Attack MISS! Consecutive misses: "
                                      << octopus->consecutiveMisses << std::endl;

                            if (octopus->consecutiveMisses >= 3) {
                                // 3 consecutive misses -- stop attacking, close distance
                                octopus->consecutiveMisses = 0;
                                octopus->state = OctopusState::MOVING;
                                octopus->animElapsedTime = 0.0f;
                                std::cout << "[Octopus] 3 misses -- switching to MOVING to close distance" << std::endl;
                            } else {
                                // Less than 3 misses -- try attacking again
                                octopus->animElapsedTime = 0.0f;
                                setAnimation(animator, "Mon_PiratesKing_Attack01", false);
                                octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Attack01");
                            }
                        }
                    }
                }
                // ==============================================================
                // STEP 7: MOVING (close distance toward player)
                // ==============================================================
                else if (octopus->state == OctopusState::MOVING) {
                    targetY = octopus->surfacedY;

                    if (player) {
                        glm::vec3 toPlayer = player->localTransform.position - pos;
                        toPlayer.y = 0.0f;
                        float dist = glm::length(toPlayer);
                        float rotY = entity->localTransform.rotation.y;

                        // Check if player is now in attack range AND cone -- transition back to ATTACKING
                        if (dist <= octopus->attackRange &&
                            isInAttackCone(pos, rotY, player->localTransform.position, octopus->attackRange)) {
                            octopus->state = OctopusState::ATTACKING;
                            octopus->animElapsedTime = 0.0f;
                            octopus->currentAnimIndex = OctopusAnimation::ATTACK;
                            setAnimation(animator, "Mon_PiratesKing_Attack01", false);
                            octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Attack01");
                            std::cout << "[Octopus] In range -- switching to ATTACKING" << std::endl;
                        } else {
                            // --------------------------------------------------
                            // Move toward player (range+cone check above handles stopping)
                            // --------------------------------------------------
                            glm::vec3 dir = glm::normalize(toPlayer);
                            pos += dir * octopus->chaseSpeed * deltaTime;

                            // Choose movement animation based on signed angle to player
                            float angleDeg = getSignedAngleToPlayer(pos, rotY, player->localTransform.position);

                            std::string moveClip;
                            if (angleDeg >= -30.0f && angleDeg <= 30.0f) {
                                // Player is within +/- 30 degrees forward -- walk forward
                                moveClip = "Mon_PiratesKing_Walk01";
                                octopus->currentAnimIndex = OctopusAnimation::RUN_FORWARD;
                            } else if (angleDeg < -30.0f) {
                                // Player is to the left -- turn left
                                moveClip = "Mon_PiratesKing_Turn_L01";
                                octopus->currentAnimIndex = OctopusAnimation::MOVE_LEFT;
                            } else {
                                // Player is to the right -- turn right
                                moveClip = "Mon_PiratesKing_Turn_R01";
                                octopus->currentAnimIndex = OctopusAnimation::MOVE_RIGHT;
                            }

                            // Only switch animation if it changed (avoid resetting every frame)
                            if (animator && animator->currentAnimIndex != (int)octopus->currentAnimIndex) {
                                setAnimation(animator, moveClip, true);
                            }
                        }
                    }
                }
                // ==============================================================
                // STEP 8: ENRAGED
                // ==============================================================
                else if (octopus->state == OctopusState::ENRAGED) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::SKILL_DANCE_HALF_HEALTH;
                }
                // ==============================================================
                // STEP 9: DYING
                // ==============================================================
                else if (octopus->state == OctopusState::DYING) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::DEATH;
                }

                // Smooth Y interpolation (skip during SURFACING which has its own lerp)
                if (octopus->state != OctopusState::SURFACING) {
                    pos.y += (targetY - pos.y) * 2.0f * deltaTime;
                }

                // Sync animator if animation index changed (safety fallback)
                if (animator && animator->currentAnimIndex != (int)octopus->currentAnimIndex) {
                    animator->currentAnimIndex = (int)octopus->currentAnimIndex;
                    animator->currentAnimationTime = 0.0f;
                }
            }
        }
    };
}