#pragma once

#include <ecs/world.hpp>
#include <components/elgembellias-component.hpp>
#include <components/animator.hpp>
#include <components/mesh-renderer.hpp>
#include <components/health.hpp>
#include <asset-loader.hpp>
#include <iostream>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

namespace our {

    class ElgembelliasSystem {
    public:

        // Helper: set animation — only changes index if different (prevents resetting anim time)
        void setAnim(Entity* entity, AnimatorComponent* anim, ElgembelliasAnimation animIndex, bool loop = true) {
            if (anim) {
                if (anim->currentAnimIndex != (int)animIndex) {
                    anim->currentAnimIndex = (int)animIndex;
                }
                anim->loopAnimation = loop;
            }
            auto comp = entity->getComponent<ElgembelliasComponent>();
            if (comp) comp->currentAnimIndex = animIndex;
        }

        // Helper: face entity toward target position (Y-axis rotation only)
        void facePosition(Entity* entity, const glm::vec3& target) {
            glm::vec3 diff = target - entity->localTransform.position;
            diff.y = 0.0f;
            if (glm::length(diff) > 0.01f) {
                entity->localTransform.rotation.y = atan2(diff.x, diff.z);
            }
        }

        // Activates the Elgembellias entity (already deserialized from JSON)
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
                comp->currentHealth = comp->maxHealth;
                comp->previousHealth = comp->maxHealth;
                comp->hitCounter = 0;
                comp->forceReposition = false;
                comp->justDamaged = false;
                comp->damageFlashTimer = 0.0f;

                // Start dance animation
                auto anim = entity->getComponent<AnimatorComponent>();
                setAnim(entity, anim, ElgembelliasAnimation::DANCE, true);

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

                // ==============================================================
                // DAMAGE DETECTION: interrupt any state (except DEATH) when hit
                // ==============================================================
                if (comp->currentHealth < comp->previousHealth &&
                    comp->state != ElgembelliasState::DEATH) {
                    comp->justDamaged = true;
                    comp->damageFlashTimer = 0.3f;
                    comp->state = ElgembelliasState::DAMAGE;
                    comp->stateTimer = 0.0f;
                    comp->animElapsedTime = 0.0f;
                    setAnim(entity, anim, ElgembelliasAnimation::DAMAGE, false);
                    std::cout << "[Elgembellias] DAMAGED! Interrupting to flinch" << std::endl;
                }
                comp->previousHealth = comp->currentHealth;

                // Death check
                if (comp->currentHealth <= 0.0f && comp->state != ElgembelliasState::DEATH) {
                    comp->state = ElgembelliasState::DEATH;
                    comp->stateTimer = 0.0f;
                    comp->animElapsedTime = 0.0f;
                    setAnim(entity, anim, ElgembelliasAnimation::DEATH, false);
                    std::cout << "[Elgembellias] DEATH!" << std::endl;
                    break;
                }

                comp->animElapsedTime += deltaTime;

                // Decay damage flash
                if (comp->damageFlashTimer > 0.0f)
                    comp->damageFlashTimer -= deltaTime;

                glm::vec3 pos = entity->localTransform.position;

                switch (comp->state) {

                    // === HIDDEN: Start surfacing ===
                    case ElgembelliasState::HIDDEN: {
                        comp->state = ElgembelliasState::SURFACING;
                        comp->stateTimer = 0.0f;
                        comp->animElapsedTime = 0.0f;
                        std::cout << "[Elgembellias] Starting to surface" << std::endl;
                        break;
                    }

                    // === SURFACING: Rise from underwater while dancing ===
                    case ElgembelliasState::SURFACING: {
                        comp->stateTimer += deltaTime;
                        setAnim(entity, anim, ElgembelliasAnimation::DANCE, true);

                        float totalDuration = glm::abs(comp->surfacedY - comp->submergedY) / comp->surfaceSpeed;
                        float t = glm::min(comp->stateTimer / totalDuration, 1.0f);
                        entity->localTransform.position.y = glm::mix(comp->submergedY, comp->surfacedY, t);

                        if (player) facePosition(entity, player->localTransform.position);

                        if (t >= 1.0f) {
                            entity->localTransform.position.y = comp->surfacedY;
                            comp->state = ElgembelliasState::DANCING;
                            comp->stateTimer = 0.0f;
                            comp->danceTimer = 0.0f;
                            comp->animElapsedTime = 0.0f;
                            std::cout << "[Elgembellias] Surfaced! Starting dance" << std::endl;
                        }
                        break;
                    }

                    // === DANCING: Face player, dance, then chase ===
                    case ElgembelliasState::DANCING: {
                        comp->stateTimer += deltaTime;
                        comp->danceTimer += deltaTime;

                        if (player) facePosition(entity, player->localTransform.position);
                        setAnim(entity, anim, ElgembelliasAnimation::DANCE, true);

                        if (comp->danceTimer >= comp->danceDuration) {
                            comp->state = ElgembelliasState::CHASE;
                            comp->stateTimer = 0.0f;
                            comp->animElapsedTime = 0.0f;
                            setAnim(entity, anim, ElgembelliasAnimation::RUN, true);
                            std::cout << "[Elgembellias] Dance done, chasing player" << std::endl;
                        }
                        break;
                    }

                    // === CHASE: Run toward player (like octopus MOVING) ===
                    case ElgembelliasState::CHASE: {
                        if (!player) break;

                        glm::vec3 toPlayer = player->localTransform.position - pos;
                        toPlayer.y = 0.0f;
                        float dist = glm::length(toPlayer);

                        facePosition(entity, player->localTransform.position);

                        // forceReposition: after a miss, must close to minDistance first
                        if (comp->forceReposition && dist <= comp->minDistance) {
                            comp->forceReposition = false;
                            std::cout << "[Elgembellias] Repositioned, ready to attack" << std::endl;
                        }

                        // Can we attack? (in range AND not forced to reposition)
                        bool canAttack = (dist <= comp->attackRange) && !comp->forceReposition;

                        if (canAttack) {
                            // In range -> start kick
                            comp->state = ElgembelliasState::ATTACKING;
                            comp->stateTimer = 0.0f;
                            comp->animElapsedTime = 0.0f;
                            comp->hitRegisteredThisKick = false;
                            comp->isAttackActive = false;
                            setAnim(entity, anim, ElgembelliasAnimation::KICK, false);
                            std::cout << "[Elgembellias] In range, KICK!" << std::endl;
                        } else {
                            // Move toward player (but don't get closer than minDistance)
                            if (dist > comp->minDistance) {
                                glm::vec3 dir = glm::normalize(toPlayer);
                                entity->localTransform.position += dir * comp->chaseSpeed * deltaTime;
                                entity->localTransform.position.y = comp->surfacedY;
                            }
                            // Run animation (looping) — only switch if not already playing
                            if (anim && anim->currentAnimIndex != (int)ElgembelliasAnimation::RUN) {
                                setAnim(entity, anim, ElgembelliasAnimation::RUN, true);
                            }
                        }
                        break;
                    }

                    // === ATTACKING: Kick animation + hit detection ===
                    case ElgembelliasState::ATTACKING: {
                        comp->stateTimer += deltaTime;

                        if (player) facePosition(entity, player->localTransform.position);

                        // Compute normalized animation time
                        float normTime = (comp->currentAnimDuration > 0.0f)
                            ? comp->animElapsedTime / comp->currentAnimDuration
                            : comp->stateTimer / comp->kickDuration;

                        // Attack is active during swing phase (0.3 to 0.8)
                        comp->isAttackActive = (normTime >= 0.3f && normTime <= 0.8f);

                        // Hit detection: sphere-sphere during active swing
                        if (comp->isAttackActive && !comp->hitRegisteredThisKick && player) {
                            glm::vec3 forward = glm::vec3(
                                sin(entity->localTransform.rotation.y), 0.0f,
                                cos(entity->localTransform.rotation.y));
                            glm::vec3 kickPos = entity->localTransform.position + forward * comp->kickHitRadius;

                            float hitDist = glm::length(glm::vec2(
                                kickPos.x - player->localTransform.position.x,
                                kickPos.z - player->localTransform.position.z));
                            float combinedRadius = comp->kickHitRadius + comp->playerRadius;

                            if (hitDist <= combinedRadius) {
                                // HIT
                                comp->hitRegisteredThisKick = true;
                                comp->hitCounter++;
                                auto hp = player->getComponent<HealthComponent>();
                                if (hp) hp->takeDamage(comp->kickDamage);
                                std::cout << "[Elgembellias] Kick HIT! hitCounter=" << comp->hitCounter
                                          << " Damage=" << comp->kickDamage << std::endl;
                            }
                        }

                        // Kick animation finished
                        if (comp->stateTimer >= comp->kickDuration) {
                            if (comp->hitRegisteredThisKick) {
                                // Hit landed
                                if (comp->hitCounter >= comp->maxHits) {
                                    // 3 consecutive hits -> chickenDance
                                    comp->state = ElgembelliasState::DANCE_2;
                                    comp->stateTimer = 0.0f;
                                    comp->animElapsedTime = 0.0f;
                                    comp->hitCounter = 0;
                                    setAnim(entity, anim, ElgembelliasAnimation::CHICKEN_DANCE, true);
                                    std::cout << "[Elgembellias] 3 hits! ChickenDance" << std::endl;
                                } else {
                                    // Hit but not 3 yet -> brief taunt, then chase
                                    comp->state = ElgembelliasState::POST_HIT;
                                    comp->stateTimer = 0.0f;
                                    comp->animElapsedTime = 0.0f;
                                    setAnim(entity, anim, ElgembelliasAnimation::DANCE, true);
                                    std::cout << "[Elgembellias] Kick hit, taunting" << std::endl;
                                }
                            } else {
                                // Missed -> taunt dance, then chase with forceReposition
                                comp->hitCounter = 0;
                                comp->state = ElgembelliasState::MISS_DANCE;
                                comp->stateTimer = 0.0f;
                                comp->animElapsedTime = 0.0f;
                                comp->forceReposition = true;
                                setAnim(entity, anim, ElgembelliasAnimation::DANCE, true);
                                std::cout << "[Elgembellias] Kick MISS! Taunting then chasing" << std::endl;
                            }
                        }
                        break;
                    }

                    // === POST_HIT: Brief taunt after landing a hit ===
                    case ElgembelliasState::POST_HIT: {
                        comp->stateTimer += deltaTime;

                        if (player) facePosition(entity, player->localTransform.position);

                        if (comp->stateTimer >= comp->postHitDanceDuration) {
                            comp->state = ElgembelliasState::CHASE;
                            comp->stateTimer = 0.0f;
                            comp->animElapsedTime = 0.0f;
                            setAnim(entity, anim, ElgembelliasAnimation::RUN, true);
                            std::cout << "[Elgembellias] Post-hit taunt done, chasing" << std::endl;
                        }
                        break;
                    }

                    // === MISS_DANCE: Brief taunt dance after a miss ===
                    case ElgembelliasState::MISS_DANCE: {
                        comp->stateTimer += deltaTime;

                        if (player) facePosition(entity, player->localTransform.position);

                        if (comp->stateTimer >= comp->missDanceDuration) {
                            comp->state = ElgembelliasState::CHASE;
                            comp->stateTimer = 0.0f;
                            comp->animElapsedTime = 0.0f;
                            setAnim(entity, anim, ElgembelliasAnimation::RUN, true);
                            std::cout << "[Elgembellias] Miss dance done, chasing" << std::endl;
                        }
                        break;
                    }

                    // === DANCE_2: ChickenDance after 3 hits ===
                    case ElgembelliasState::DANCE_2: {
                        comp->stateTimer += deltaTime;

                        if (player) facePosition(entity, player->localTransform.position);

                        if (comp->stateTimer >= comp->dance2Duration) {
                            comp->state = ElgembelliasState::CHASE;
                            comp->stateTimer = 0.0f;
                            comp->animElapsedTime = 0.0f;
                            setAnim(entity, anim, ElgembelliasAnimation::RUN, true);
                            std::cout << "[Elgembellias] ChickenDance done, chasing" << std::endl;
                        }
                        break;
                    }

                    // === DAMAGE: Flinch on hit (interrupts any state) ===
                    case ElgembelliasState::DAMAGE: {
                        comp->stateTimer += deltaTime;

                        if (comp->stateTimer >= comp->damageDuration) {
                            comp->state = ElgembelliasState::CHASE;
                            comp->stateTimer = 0.0f;
                            comp->animElapsedTime = 0.0f;
                            comp->forceReposition = true;
                            setAnim(entity, anim, ElgembelliasAnimation::RUN, true);
                            std::cout << "[Elgembellias] Damage flinch done, chasing" << std::endl;
                        }
                        break;
                    }

                    // === DEATH ===
                    case ElgembelliasState::DEATH: {
                        // Hold death animation (loop=false was set on enter)
                        break;
                    }
                }

                break; // Only one Elgembellias entity
            }
        }
    };

}
