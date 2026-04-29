#pragma once

#include "ecs/world.hpp"
#include "components/octopus-component.hpp"
#include "components/health.hpp"
#include "components/animator.hpp"
#include "ecs/transform.hpp"
#include "mesh/model.hpp"
#include "application.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <iostream>
#include <unordered_map>
#include <tuple>
#include <algorithm>
#include <map>

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
                {"Mon_PiratesKing_skill01",     (int)OctopusAnimation::ENRAGE_DANCE},
                {"Mon_PiratesKing_Skill04_Start", (int)OctopusAnimation::SKILL04_START},
                {"Mon_PiratesKing_Skill04_Loop", (int)OctopusAnimation::SKILL04_LOOP},
                {"Mon_PiratesKing_Skill04_End", (int)OctopusAnimation::SKILL04_END},
                {"Mon_PiratesKing_Attack02",    (int)OctopusAnimation::ATTACK02},
                {"Mon_PiratesKing_Death",       (int)OctopusAnimation::DEATH},
                {"Mon_PiratesKing_skill03",     (int)OctopusAnimation::SKILL_VICTORY},
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
            animator->loopAnimation = loop;
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
                {"Mon_PiratesKing_skill01",     (int)OctopusAnimation::ENRAGE_DANCE},
                {"Mon_PiratesKing_Skill04_Start", (int)OctopusAnimation::SKILL04_START},
                {"Mon_PiratesKing_Skill04_Loop", (int)OctopusAnimation::SKILL04_LOOP},
                {"Mon_PiratesKing_Skill04_End", (int)OctopusAnimation::SKILL04_END},
                {"Mon_PiratesKing_Attack02",    (int)OctopusAnimation::ATTACK02},
                {"Mon_PiratesKing_Death",       (int)OctopusAnimation::DEATH},
                {"Mon_PiratesKing_skill03",     (int)OctopusAnimation::SKILL_VICTORY},
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

        // ------------------------------------------------------------------
        // buildTentacleChains: discovers tentacle bone chains from the model's
        // bone info map. Groups bones by prefix, sorts by suffix number.
        // Called once on first frame when the boss has a valid animator.
        // ------------------------------------------------------------------
        static void buildTentacleChains(OctopusComponent* octopus, AnimatorComponent* animator) {
            if (!animator || animator->modelName.empty()) return;
            Model* m = ModelLoader::models[animator->modelName];
            if (!m) return;

            auto& boneInfoMap = m->getBoneInfoMap();

            // Temporary: prefix -> [(suffixNum, boneIndex, offsetMatrix)]
            struct TempBone { int suffix; int idx; glm::mat4 off; };
            std::map<std::string, std::vector<TempBone>> chainBones;

            for (auto& [name, info] : boneInfoMap) {
                // Only process bones with "Tentacle" in their name
                if (name.find("Tentacle") == std::string::npos) continue;

                // Extract prefix and suffix number
                // e.g. "BN_PiratesKing_Tentacle_R_01" -> prefix="BN_PiratesKing_Tentacle_R_", suffix=1
                size_t lastUnderscore = name.rfind('_');
                if (lastUnderscore == std::string::npos || lastUnderscore + 1 >= name.size()) continue;

                std::string prefix = name.substr(0, lastUnderscore + 1);
                std::string suffixStr = name.substr(lastUnderscore + 1);

                int suffixNum = 0;
                try { suffixNum = std::stoi(suffixStr); }
                catch (...) { continue; } // Skip bones without numeric suffix

                chainBones[prefix].push_back({suffixNum, info.id, info.offset});
            }

            // Build sorted chains
            for (auto& [prefix, bones] : chainBones) {
                std::sort(bones.begin(), bones.end(),
                          [](const auto& a, const auto& b) { return a.suffix < b.suffix; });

                OctopusComponent::TentacleChain chain;
                chain.prefix = prefix;
                for (auto& b : bones) {
                    OctopusComponent::TentacleChainBone cb;
                    cb.boneIndex = b.idx;
                    cb.inverseOffset = glm::inverse(b.off);
                    chain.bones.push_back(cb);
                }
                octopus->tentacleChains.push_back(std::move(chain));
            }

            octopus->tentacleChainsBuilt = true;

            // Debug: print discovered chains
            std::cout << "[Octopus] Built " << octopus->tentacleChains.size() << " tentacle chains:" << std::endl;
            for (auto& chain : octopus->tentacleChains) {
                std::cout << "  " << chain.prefix << " -> " << chain.bones.size() << " bones" << std::endl;
            }
        }

        // ------------------------------------------------------------------
        // updateTentacleBonePositions: computes world-space positions for all
        // tentacle bones using the final bone matrices and model world transform.
        //
        // Bone world position = modelWorldMatrix * finalBonesMatrices[idx] * inverseOffset * vec4(0,0,0,1)
        //
        // Explanation of the math:
        //   finalBonesMatrices[idx] = globalInverseTransform * globalAnimatedTransform * offset
        //   This skin matrix transforms bind-pose mesh vertices to animated model-space positions.
        //   To get the bone's HEAD position (not a vertex), we need:
        //     globalInverseTransform * globalAnimatedTransform * vec4(0,0,0,1)
        //   Which equals:
        //     finalBonesMatrices[idx] * inverse(offset) * vec4(0,0,0,1)
        //   Then modelWorldMatrix transforms from model space to world space.
        // ------------------------------------------------------------------
      static void updateTentacleBonePositions(OctopusComponent* octopus,
                                         AnimatorComponent* animator,
                                         const glm::mat4& modelWorldMatrix) {
    if (!animator || animator->finalBonesMatrices.empty()) return;

    octopus->tentacle_tip_positions.clear();

    for (auto& chain : octopus->tentacleChains) {
        chain.worldPositions.clear();
        for (auto& bone : chain.bones) {
            if (bone.boneIndex >= 0 &&
                bone.boneIndex < (int)animator->finalBonesMatrices.size()) {

                // finalBonesMatrices[i] = globalInverseTransform * globalBoneTransform * offset
                // To get bone HEAD position: multiply by inverseOffset to cancel the offset
                // Then modelWorldMatrix transforms from model space to world space
                glm::vec4 boneModelPos = animator->finalBonesMatrices[bone.boneIndex]
                                         * bone.inverseOffset
                                         * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

                glm::vec3 boneWorldPos = glm::vec3(modelWorldMatrix * boneModelPos);
                chain.worldPositions.push_back(boneWorldPos);
            }
        }
        if (!chain.worldPositions.empty()) {
            octopus->tentacle_tip_positions.push_back(chain.worldPositions.back());
        }
    }
}

        // ------------------------------------------------------------------
        // checkTentacleHit: checks if any tentacle segment collides with the
        // player. Each pair of consecutive bones in a chain forms a line segment
        // (capsule). Returns true if any segment is within (tentacleRadius + playerRadius)
        // of the player center. Outputs the world-space hit position.
        //
        // Closest-point-on-segment formula:
        //   Given segment AB and point P:
        //     t = dot(AP, AB) / dot(AB, AB)   -- parameter along segment
        //     t = clamp(t, 0, 1)              -- clamp to segment bounds
        //     closest = A + t * AB             -- closest point on segment
        //     distance = length(closest - P)   -- distance to player center
        // ------------------------------------------------------------------
        static bool checkTentacleHit(OctopusComponent* octopus,
                                      const glm::vec3& playerPos,
                                      float playerRadius,
                                      glm::vec3& outHitPos) {
            float hitThreshold = octopus->tentacleRadius + playerRadius;

            for (auto& chain : octopus->tentacleChains) {
                // Check each consecutive pair of bones as a capsule segment
                for (size_t i = 0; i + 1 < chain.worldPositions.size(); i++) {
                    glm::vec3 a = chain.worldPositions[i];
                    glm::vec3 b = chain.worldPositions[i + 1];

                    // Closest point on segment AB to player center P
                    glm::vec3 ab = b - a;
                    glm::vec3 ap = playerPos - a;
                    float abLenSq = glm::dot(ab, ab);
                    // t = parameter along segment [0,1]
                    float t = (abLenSq > 0.0001f) ? glm::dot(ap, ab) / abLenSq : 0.0f;
                    t = glm::clamp(t, 0.0f, 1.0f);
                    glm::vec3 closest = a + t * ab;

                    float dist = glm::length(closest - playerPos);
                    if (dist < hitThreshold) {
                        outHitPos = closest;
                        return true;
                    }
                }
            }
            return false;
        }

        // ------------------------------------------------------------------
        // Wave effect helpers for Attack02
        // ------------------------------------------------------------------
        static void spawnWave(OctopusComponent* octopus, const glm::vec3& origin, const glm::vec3& playerPos) {
            octopus->waveActive = true;
            octopus->waveOrigin = glm::vec3(origin.x, 0.05f, origin.z);
            octopus->waveOuterRadius = 2.5f;  // center radius starts small
            octopus->waveInnerRadius = 0.1f; // will be set to outer - 2.4 in updateWave
            octopus->waveHasDealtDamage = false;
            octopus->waveFired = true;
            octopus->waveAlpha = 0.75f;
            float distToPlayer = glm::length(glm::vec2(playerPos.x - origin.x, playerPos.z - origin.z));
            octopus->waveMaxRadius = distToPlayer + 15.0f;
        }

        static void updateWave(OctopusComponent* octopus, Entity* player, float deltaTime) {
            if (!octopus->waveActive) return;

            // waveOuterRadius is the CENTER radius of the ring (expands 0 -> maxRadius)
            octopus->waveOuterRadius += octopus->waveExpandSpeed * deltaTime;
            // Fixed 2.4-unit wide ring: inner = center - 1.2, outer = center + 1.2
            octopus->waveInnerRadius = octopus->waveOuterRadius - 1.2f;
            if (octopus->waveInnerRadius < 0.0f) octopus->waveInnerRadius = 0.0f;

            float t = octopus->waveOuterRadius / octopus->waveMaxRadius;
            octopus->waveAlpha = 0.75f * (1.0f - t);
            octopus->waveAlpha = glm::clamp(octopus->waveAlpha, 0.0f, 0.75f);

            if (player && !octopus->waveHasDealtDamage) {
                glm::vec3 pPos = player->localTransform.position;
                float dist = glm::length(glm::vec2(pPos.x - octopus->waveOrigin.x,
                                                    pPos.z - octopus->waveOrigin.z));
                // Hit band: ring width ±1.0 unit tolerance
                bool inRing = (dist >= octopus->waveOuterRadius - 2.2f &&
                               dist <= octopus->waveOuterRadius + 2.2f);
                if (inRing && std::abs(pPos.y - octopus->waveOrigin.y) < 3.0f) {
                    auto playerHealth = player->getComponent<HealthComponent>();
                    if (playerHealth) {
                        playerHealth->takeDamage(octopus->waveDamage);
                        std::cout << "[Octopus] Wave HIT player for "
                                  << octopus->waveDamage << " damage!" << std::endl;
                    }
                    octopus->waveHasDealtDamage = true;
                    octopus->screenWaterTimer = octopus->screenWaterDuration;
                }
            }

            if (octopus->waveOuterRadius >= octopus->waveMaxRadius) {
                octopus->waveActive = false;
            }
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
                auto healthComp = entity->getComponent<HealthComponent>();
                glm::vec3& pos = entity->localTransform.position;
                float targetY = octopus->submergedY;

                // Face the player at all times during combat states (before state logic)
                // But NOT when dead/dying/death-anim/reviving
                if (player && octopus->state != OctopusState::DEATH_ANIM &&
                    octopus->state != OctopusState::REVIVING &&
                    octopus->state != OctopusState::DYING) {
                    glm::vec3 diff = player->localTransform.position - pos;
                    diff.y = 0.0f;
                    if (glm::length(diff) > 0.01f) entity->localTransform.rotation.y = atan2(diff.x, diff.z);
                }

                // Build tentacle chains once (after model is loaded and animation system has populated bone info)
                if (!octopus->tentacleChainsBuilt && animator && !animator->modelName.empty()) {
                    buildTentacleChains(octopus, animator);
                }
                // Update tentacle bone world positions every frame
                if (octopus->tentacleChainsBuilt && animator) {
                    glm::mat4 modelWorldMatrix = entity->getLocalToWorldMatrix();
                    updateTentacleBonePositions(octopus, animator, modelWorldMatrix);
                }
                // Tick down hit cooldown
                if (octopus->hitCooldownTimer > 0.0f) {
                    octopus->hitCooldownTimer -= deltaTime;
                }
                // Tick down stun timer
                if (octopus->stunTimer > 0.0f) {
                    octopus->stunTimer -= deltaTime;
                }

                // If stunned, skip attack/move states and go to COMBAT_IDLE
                if (octopus->stunTimer > 0.0f &&
                    (octopus->state == OctopusState::ATTACKING ||
                     octopus->state == OctopusState::ENRAGED_COMBAT ||
                     octopus->state == OctopusState::MOVING)) {
                    octopus->state = OctopusState::COMBAT_IDLE;
                    octopus->animElapsedTime = 0.0f;
                    octopus->currentAnimIndex = OctopusAnimation::COMBAT_IDLE;
                    setAnimation(animator, "Mon_PiratesKing_CombatIdle", true);
                    octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_CombatIdle");
                    std::cout << "[Octopus] STUNNED -- forced to COMBAT_IDLE" << std::endl;
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
                    if (animator) animator->loopAnimation = false;
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
                    // But NOT while stunned
                    if (octopus->bossReady && player && octopus->stunTimer <= 0.0f) {
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
                                octopus->hitRegisteredThisSwing = false;
                                octopus->isAttackActive = false;
                                octopus->hitCooldownTimer = 0.0f;
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
                // STEP 6: ATTACKING (tentacle-based hit detection)
                // ==============================================================
                else if (octopus->state == OctopusState::ATTACKING) {
                    targetY = octopus->surfacedY;

                    // Death check
                    if (healthComp && healthComp->currentHealth <= 0.0f && !octopus->permanentlyDead) {
                        octopus->state = OctopusState::DEATH_ANIM;
                        octopus->animElapsedTime = 0.0f;
                        octopus->deathAnimFinished = false;
                        setAnimation(animator, "Mon_PiratesKing_Death01", false);
                        octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Death01");
                        std::cout << "[Octopus] DEATH triggered! hasRevived=" << octopus->hasRevived << std::endl;
                        continue;
                    }

                    // Range check -- if player is out of attack range, switch to MOVING
                    if (player) {
                        glm::vec3 toPlayer = player->localTransform.position - pos;
                        toPlayer.y = 0.0f;
                        float dist = glm::length(toPlayer);
                        if (dist > octopus->attackRange) {
                            octopus->state = OctopusState::MOVING;
                            octopus->animElapsedTime = 0.0f;
                            std::cout << "[Octopus] Out of range -- switching to MOVING" << std::endl;
                            continue;
                        }
                    }

                    // Enrage check -- can only trigger once per fight
                    if (healthComp && healthComp->currentHealth <= healthComp->maxHealth * 0.5f && !octopus->has_enraged) {
                        octopus->has_enraged = true;
                        octopus->state = OctopusState::ENRAGED;
                        octopus->enrage_phase = 0;
                        octopus->animElapsedTime = 0.0f;
                        std::cout << "[Octopus] HP at 50% -- ENRAGE TRIGGERED!" << std::endl;
                    } else {
                        // Normal ATTACKING logic
                        octopus->currentAnimIndex = OctopusAnimation::ATTACK;
                        octopus->animElapsedTime += deltaTime;

                        // Compute normalized animation time (0.0 to 1.0)
                        float normTime = (octopus->currentAnimDuration > 0.0f)
                                         ? octopus->animElapsedTime / octopus->currentAnimDuration
                                         : 1.0f;

                        // Attack is "active" during the swing phase (normTime 0.3 to 0.8)
                        octopus->isAttackActive = (normTime >= 0.3f && normTime <= 0.8f);

                        // Check tentacle hits ONLY during active swing phase
                        if (octopus->isAttackActive && !octopus->hitRegisteredThisSwing
                            && octopus->hitCooldownTimer <= 0.0f && player) {
                            glm::vec3 hitPos;
                            if (checkTentacleHit(octopus, player->localTransform.position,
                                                 octopus->playerRadius, hitPos)) {
                                // TENTACLE HIT: deal damage, set cooldown
                                octopus->hitRegisteredThisSwing = true;
                                octopus->hitCooldownTimer = octopus->hitCooldownDuration;
                                octopus->lastHitPosition = hitPos;
                                octopus->consecutiveMisses = 0;
                                octopus->attackCount++;
                                auto playerHealth = player->getComponent<HealthComponent>();
                                if (playerHealth) {
                                    playerHealth->takeDamage(octopus->attackDamage);
                                    std::cout << "[Octopus] Tentacle HIT! Dealt " << octopus->attackDamage
                                              << " damage at (" << hitPos.x << "," << hitPos.y << "," << hitPos.z << ")"
                                              << ". Total attacks: " << octopus->attackCount << std::endl;
                                }
                            }
                        }

                        // When attack animation finishes, decide next state
                        if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                            if (octopus->hitRegisteredThisSwing) {
                                // --------------------------------------------------
                                // HIT this swing: restart attack cycle
                                // --------------------------------------------------
                                octopus->hitRegisteredThisSwing = false;
                                octopus->isAttackActive = false;
                                octopus->animElapsedTime = 0.0f;
                                setAnimation(animator, "Mon_PiratesKing_Attack01", false);
                                octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Attack01");
                            } else {
                                // --------------------------------------------------
                                // MISS this swing: increment miss count
                                // --------------------------------------------------
                                octopus->isAttackActive = false;
                                octopus->consecutiveMisses++;
                                std::cout << "[Octopus] Attack MISS! Consecutive misses: "
                                          << octopus->consecutiveMisses << std::endl;

                                if (octopus->consecutiveMisses >= 3) {
                                    // 3 consecutive misses -- stop attacking, close distance
                                    octopus->consecutiveMisses = 0;
                                    octopus->hitRegisteredThisSwing = false;
                                    octopus->state = OctopusState::MOVING;
                                    octopus->animElapsedTime = 0.0f;
                                    octopus->force_reposition = true;
                                    std::cout << "[Octopus] 3 misses -- switching to MOVING to close distance" << std::endl;
                                    std::cout << "Boss repositioning \xE2\x80\x94 closing gap before next attack" << std::endl;
                                } else {
                                    // Less than 3 misses -- try attacking again
                                    octopus->animElapsedTime = 0.0f;
                                    setAnimation(animator, "Mon_PiratesKing_Attack01", false);
                                    octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Attack01");
                                }
                            }
                        }
                    }
                }
                // ==============================================================
                // STEP 7: MOVING (close distance toward player)
                // ==============================================================
                else if (octopus->state == OctopusState::MOVING) {
                    targetY = octopus->surfacedY;

                    // Death check
                    if (healthComp && healthComp->currentHealth <= 0.0f && !octopus->permanentlyDead) {
                        octopus->state = OctopusState::DEATH_ANIM;
                        octopus->animElapsedTime = 0.0f;
                        octopus->deathAnimFinished = false;
                        setAnimation(animator, "Mon_PiratesKing_Death01", false);
                        octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Death01");
                        std::cout << "[Octopus] DEATH triggered! hasRevived=" << octopus->hasRevived << std::endl;
                        continue;
                    }

                    // Enrage check -- can only trigger once per fight
                    if (healthComp && healthComp->currentHealth <= healthComp->maxHealth * 0.5f && !octopus->has_enraged) {
                        octopus->has_enraged = true;
                        octopus->state = OctopusState::ENRAGED;
                        octopus->enrage_phase = 0;
                        octopus->animElapsedTime = 0.0f;
                        std::cout << "[Octopus] HP at 50% -- ENRAGE TRIGGERED!" << std::endl;
                    } else if (player) {
                        glm::vec3 toPlayer = player->localTransform.position - pos;
                        toPlayer.y = 0.0f;
                        float dist = glm::length(toPlayer);
                        float rotY = entity->localTransform.rotation.y;

                        if (octopus->force_reposition) {
                            if (dist < octopus->minFollowDistance) {
                                octopus->force_reposition = false;
                            }
                        }

                        // Check if player is now in attack range AND cone -- transition back to attacking
                        if (!octopus->force_reposition && dist <= octopus->attackRange &&
                            isInAttackCone(pos, rotY, player->localTransform.position, octopus->attackRange)) {

                            // Common setup for both attack states
                            octopus->animElapsedTime = 0.0f;
                            octopus->hitRegisteredThisSwing = false;
                            octopus->isAttackActive = false;
                            octopus->hitCooldownTimer = 0.0f;

                            if (octopus->has_enraged) {
                                // After enrage: return to enraged combat mode
                                octopus->state = OctopusState::ENRAGED_COMBAT;
                                std::cout << "[Octopus] In range -- switching to ENRAGED_COMBAT" << std::endl;
                            } else {
                                // Normal combat: return to ATTACKING
                                octopus->state = OctopusState::ATTACKING;
                                octopus->currentAnimIndex = OctopusAnimation::ATTACK;
                                setAnimation(animator, "Mon_PiratesKing_Attack01", false);
                                octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Attack01");
                                std::cout << "[Octopus] In range -- switching to ATTACKING" << std::endl;
                            }
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
                // STEP 8: ENRAGED (enrage animation sequence)
                // ==============================================================
                else if (octopus->state == OctopusState::ENRAGED) {
                    targetY = octopus->surfacedY;
                    octopus->force_reposition = false;
                    octopus->animElapsedTime += deltaTime;

                    switch (octopus->enrage_phase) {
                        case 0: // skill01 (one-shot)
                            octopus->currentAnimIndex = OctopusAnimation::ENRAGE_DANCE;
                            if (animator && animator->currentAnimIndex != (int)octopus->currentAnimIndex) {
                                setAnimation(animator, "Mon_PiratesKing_skill01", false);
                                octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_skill01");
                            }
                            if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                                octopus->enrage_phase = 1;
                                octopus->animElapsedTime = 0.0f;
                            }
                            break;

                        case 1: // Skill04_Start (one-shot)
                            octopus->currentAnimIndex = OctopusAnimation::SKILL04_START;
                            if (animator && animator->currentAnimIndex != (int)octopus->currentAnimIndex) {
                                setAnimation(animator, "Mon_PiratesKing_Skill04_Start", false);
                                octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Skill04_Start");
                            }
                            if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                                octopus->enrage_phase = 2;
                                octopus->animElapsedTime = 0.0f;
                                octopus->currentAnimDuration = 3.0f; // 3 seconds for loop phase
                            }
                            break;

                        case 2: // Skill04_Loop (loop for 3.0 seconds)
                            octopus->currentAnimIndex = OctopusAnimation::SKILL04_LOOP;
                            if (animator && animator->currentAnimIndex != (int)octopus->currentAnimIndex) {
                                setAnimation(animator, "Mon_PiratesKing_Skill04_Loop", true);
                            }
                            if (octopus->animElapsedTime >= 3.0f) {
                                octopus->enrage_phase = 3;
                                octopus->animElapsedTime = 0.0f;
                            }
                            break;

                        case 3: // Skill04_End (one-shot)
                            octopus->currentAnimIndex = OctopusAnimation::SKILL04_END;
                            if (animator && animator->currentAnimIndex != (int)octopus->currentAnimIndex) {
                                setAnimation(animator, "Mon_PiratesKing_Skill04_End", false);
                                octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Skill04_End");
                            }
                            if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                                // Sequence complete -- transition to ENRAGED_COMBAT
                                octopus->enrage_phase = 0;
                                octopus->animElapsedTime = 0.0f;
                                octopus->state = OctopusState::ENRAGED_COMBAT;
                                octopus->attack_combo_index = 0;
                                octopus->consecutiveMisses = 0;
                                octopus->hitRegisteredThisSwing = false;
                                octopus->isAttackActive = false;
                                octopus->hitCooldownTimer = 0.0f;
                                std::cout << "[Octopus] Enrage sequence complete -- entering ENRAGED_COMBAT!" << std::endl;
                            }
                            break;
                    }
                }
                // ==============================================================
                // STEP 10: ENRAGED_COMBAT (enraged attack with combo pattern)
                // ==============================================================
                else if (octopus->state == OctopusState::ENRAGED_COMBAT) {
                    targetY = octopus->surfacedY;
                    octopus->force_reposition = false;

                    // Death check
                    if (healthComp && healthComp->currentHealth <= 0.0f && !octopus->permanentlyDead) {
                        octopus->state = OctopusState::DEATH_ANIM;
                        octopus->animElapsedTime = 0.0f;
                        octopus->deathAnimFinished = false;
                        setAnimation(animator, "Mon_PiratesKing_Death01", false);
                        octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Death01");
                        std::cout << "[Octopus] DEATH triggered! hasRevived=" << octopus->hasRevived << std::endl;
                        continue;
                    }

                    // Range check -- if player is out of attack range, switch to MOVING
                    if (player) {
                        glm::vec3 toPlayer = player->localTransform.position - pos;
                        toPlayer.y = 0.0f;
                        float dist = glm::length(toPlayer);
                        if (dist > octopus->attackRange) {
                            octopus->state = OctopusState::MOVING;
                            octopus->animElapsedTime = 0.0f;
                            std::cout << "[Octopus] Out of range -- switching to MOVING" << std::endl;
                            continue;
                        }
                    }

                    // Determine attack clip based on combo index
                    std::string attackClip;
                    if (octopus->attack_combo_index == 4) {
                        attackClip = "Mon_PiratesKing_Attack02";
                        octopus->currentAnimIndex = OctopusAnimation::ATTACK02;
                    } else {
                        attackClip = "Mon_PiratesKing_Attack01";
                        octopus->currentAnimIndex = OctopusAnimation::ATTACK;
                    }

                    // Set animation if needed (first entry or restart)
                    if (animator && animator->currentAnimIndex != (int)octopus->currentAnimIndex) {
                        setAnimation(animator, attackClip, false);
                        octopus->currentAnimDuration = queryAnimDuration(animator, attackClip);
                    }

                    // Reset waveFired at the START of each Attack02 cycle
                    // (must be outside animation-change check because setAnimation
                    //  is also called at end of previous combo, so by next frame
                    //  currentAnimIndex already matches and the check above is false)
                    if (attackClip == "Mon_PiratesKing_Attack02" && !octopus->waveActive && octopus->waveFired) {
                        octopus->waveFired = false;
                    }

                    octopus->animElapsedTime += deltaTime;

                    // Attack02 wave triggers at 40% through animation (mid-slam)
                    if (attackClip == "Mon_PiratesKing_Attack02" && !octopus->waveFired && player) {
                        float triggerTime = octopus->currentAnimDuration * 0.40f;
                        if (octopus->animElapsedTime >= triggerTime) {
                            spawnWave(octopus, pos, player->localTransform.position);
                            octopus->waveFired = true;
                        }
                    }

                    // Compute normalized animation time (0.0 to 1.0)
                    float normTime = (octopus->currentAnimDuration > 0.0f)
                                     ? octopus->animElapsedTime / octopus->currentAnimDuration
                                     : 1.0f;

                    // Attack is "active" during the swing phase (normTime 0.3 to 0.8)
                    octopus->isAttackActive = (normTime >= 0.3f && normTime <= 0.8f);

                    // Check tentacle hits ONLY during active swing phase
                    if (octopus->isAttackActive && !octopus->hitRegisteredThisSwing
                        && octopus->hitCooldownTimer <= 0.0f && player) {
                        glm::vec3 hitPos;
                        if (checkTentacleHit(octopus, player->localTransform.position,
                                             octopus->playerRadius, hitPos)) {
                            octopus->hitRegisteredThisSwing = true;
                            octopus->hitCooldownTimer = octopus->hitCooldownDuration;
                            octopus->lastHitPosition = hitPos;
                            octopus->consecutiveMisses = 0;
                            octopus->attackCount++;
                            auto playerHealth = player->getComponent<HealthComponent>();
                            if (playerHealth) {
                                playerHealth->takeDamage(octopus->attackDamage);
                                std::cout << "[Octopus] Enraged Tentacle HIT! Dealt " << octopus->attackDamage
                                          << ". Total attacks: " << octopus->attackCount << std::endl;
                            }
                        }
                    }

                    // When attack animation finishes, decide next state
                    if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                        bool isAttack02Resolved = false;
                        if (attackClip == "Mon_PiratesKing_Attack02") {
                            bool playerHit = octopus->hitRegisteredThisSwing;
                            bool waterHit = false;
                            for (auto& chain : octopus->tentacleChains) {
                                for (auto& tPos : chain.worldPositions) {
                                    if (tPos.y <= octopus->surfacedY + 1.0f) {
                                        waterHit = true;
                                        break;
                                    }
                                }
                                if (waterHit) break;
                            }

                            // Attack02 outcome resolved (wave already spawned at 40%)
                            isAttack02Resolved = true;

                            if (playerHit && !waterHit) {
                                std::cout << "[Octopus] Attack02 Outcome: 1 - direct hit + wave" << std::endl;
                                auto playerHealth = player->getComponent<HealthComponent>();
                                if (playerHealth) {
                                    playerHealth->takeDamage(octopus->attackDamage * 0.5f);
                                }
                            } else if (playerHit && waterHit) {
                                auto playerHealth = player->getComponent<HealthComponent>();
                                if (playerHealth) {
                                    playerHealth->takeDamage(octopus->attackDamage * 0.5f);
                                }
                                std::cout << "[Octopus] Attack02 Outcome: 2 - hit + wave" << std::endl;
                            } else if (!playerHit && waterHit) {
                                std::cout << "[Octopus] Attack02 Outcome: 3 - wave only" << std::endl;
                            } else {
                                std::cout << "[Octopus] Attack02 Outcome: 4 - wave only (no contact)" << std::endl;
                            }
                        }

                        if (octopus->hitRegisteredThisSwing || isAttack02Resolved) {
                            octopus->hitRegisteredThisSwing = false;
                            octopus->isAttackActive = false;
                            octopus->animElapsedTime = 0.0f;
                            octopus->attack_combo_index = (octopus->attack_combo_index + 1) % 5;

                            std::string nextClip = (octopus->attack_combo_index == 4)
                                ? "Mon_PiratesKing_Attack02" : "Mon_PiratesKing_Attack01";
                            octopus->currentAnimIndex = (octopus->attack_combo_index == 4)
                                ? OctopusAnimation::ATTACK02 : OctopusAnimation::ATTACK;
                            setAnimation(animator, nextClip, false);
                            octopus->currentAnimDuration = queryAnimDuration(animator, nextClip);
                        } else {
                            octopus->isAttackActive = false;
                            octopus->consecutiveMisses++;
                            std::cout << "[Octopus] Enraged Attack MISS! Consecutive misses: "
                                      << octopus->consecutiveMisses << std::endl;

                            if (octopus->consecutiveMisses >= 3) {
                                octopus->consecutiveMisses = 0;
                                octopus->hitRegisteredThisSwing = false;
                                octopus->force_reposition = true;
                                octopus->state = OctopusState::MOVING;
                                octopus->animElapsedTime = 0.0f;
                                std::cout << "[Octopus] 3 misses -- switching to MOVING to close distance" << std::endl;
                            } else {
                                octopus->animElapsedTime = 0.0f;
                                setAnimation(animator, attackClip, false);
                                octopus->currentAnimDuration = queryAnimDuration(animator, attackClip);
                            }
                        }
                    }
                }
                // ==============================================================
                // STEP 9: DYING (permanent death - freeze on last frame)
                // ==============================================================
                else if (octopus->state == OctopusState::DYING) {
                    targetY = -20.0f;
                    octopus->force_reposition = false;
                    octopus->currentAnimIndex = OctopusAnimation::DEATH;
                    if (animator) {
                        animator->loopAnimation = false;
                        animator->currentAnimationTime = octopus->currentAnimDuration - 0.001f;
                    }
                }
                // ==============================================================
                // DEATH_ANIM (playing death animation once)
                // ==============================================================
                else if (octopus->state == OctopusState::DEATH_ANIM) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::DEATH;
                    octopus->animElapsedTime += deltaTime;

                    if (animator) animator->loopAnimation = false;

                    if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                        if (!octopus->hasRevived) {
                            // FIRST DEATH: transition to revival
                            octopus->state = OctopusState::REVIVING;
                            octopus->animElapsedTime = 0.0f;
                            octopus->reviveAnimFinished = false;
                            setAnimation(animator, "Mon_PiratesKing_skill03", false);
                            octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_skill03");
                            std::cout << "[Octopus] Death anim done -- starting REVIVAL" << std::endl;
                        } else {
                            // SECOND DEATH: permanently dead, sink below water
                            octopus->permanentlyDead = true;
                            octopus->state = OctopusState::DYING;
                            targetY = -20.0f;
                            if (animator) {
                                animator->loopAnimation = false;
                                animator->currentAnimationTime = octopus->currentAnimDuration - 0.001f;
                            }
                            std::cout << "[Octopus] PERMANENTLY DEAD" << std::endl;
                        }
                    }
                }
                // ==============================================================
                // REVIVING (playing skill03 revival animation once)
                // ==============================================================
                else if (octopus->state == OctopusState::REVIVING) {
                    targetY = octopus->surfacedY;
                    octopus->currentAnimIndex = OctopusAnimation::SKILL_VICTORY;
                    octopus->animElapsedTime += deltaTime;

                    if (animator) animator->loopAnimation = false;

                    if (octopus->animElapsedTime >= octopus->currentAnimDuration) {
                        // Revival complete -- go directly to ENRAGED_COMBAT
                        octopus->hasRevived = true;
                        octopus->damageMultiplier = 1.75f;
                        octopus->has_enraged = true; // already enraged, skip enrage sequence
                        octopus->attack_combo_index = 0;
                        octopus->consecutiveMisses = 0;
                        octopus->force_reposition = false;
                        octopus->hitRegisteredThisSwing = false;
                        octopus->isAttackActive = false;
                        octopus->hitCooldownTimer = 0.0f;
                        octopus->bossReady = true;

                        // Restore HP using health component
                        auto reviveHealthComp = entity->getComponent<HealthComponent>();
                        if (reviveHealthComp) {
                            reviveHealthComp->currentHealth = octopus->revivalHP;
                            reviveHealthComp->maxHealth = octopus->revivalHP;
                        }

                        // Transition directly to ENRAGED_COMBAT (skip COMBAT_IDLE + enrage threshold)
                        octopus->state = OctopusState::ENRAGED_COMBAT;
                        octopus->animElapsedTime = 0.0f;
                        setAnimation(animator, "Mon_PiratesKing_Attack01", false);
                        octopus->currentAnimDuration = queryAnimDuration(animator, "Mon_PiratesKing_Attack01");
                        std::cout << "[Octopus] REVIVED! Now taking 1.75x damage. HP restored to "
                                  << octopus->revivalHP << ". Entering ENRAGED_COMBAT directly!" << std::endl;
                    }
                }

                // Update shockwave effect (runs every frame regardless of state)
                updateWave(octopus, player, deltaTime);

                // Decrement screen water distortion timer
                if (octopus->screenWaterTimer > 0.0f) {
                    octopus->screenWaterTimer -= deltaTime;
                    if (octopus->screenWaterTimer < 0.0f) octopus->screenWaterTimer = 0.0f;
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