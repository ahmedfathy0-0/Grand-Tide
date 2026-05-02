#include "combat-system.hpp"
#include "components/shark-component.hpp"
#include "components/enemy.hpp"
#include "components/health.hpp"
#include "components/animator.hpp"
#include "components/musket-component.hpp"
#include "components/marine-boat-component.hpp"
#include <glm/gtx/norm.hpp>

namespace our
{
    void CombatSystem::ensureSoundEngine() {
        if (!soundEngine) {
            soundEngine = new ma_engine();
            if (ma_engine_init(nullptr, soundEngine) != MA_SUCCESS) {
                std::cerr << "[Combat] Sound engine init failed" << std::endl;
                delete soundEngine;
                soundEngine = nullptr;
            }
        }
    }

    void CombatSystem::playSound(const char* path, float volume) {
        ensureSoundEngine();
        if (!soundEngine) return;
        ma_sound* snd = new ma_sound();
        if (ma_sound_init_from_file(soundEngine, path, MA_SOUND_FLAG_DECODE, nullptr, nullptr, snd) == MA_SUCCESS) {
            ma_sound_set_volume(snd, volume);
            ma_sound_start(snd);
        } else {
            delete snd;
        }
    }

    void CombatSystem::update(World *world, float deltaTime)
    {
        Entity *raft = nullptr;
        Entity *player = nullptr;
        Entity *boat = nullptr;

        for (auto entity : world->getEntities())
        {
            if (entity->name == "raft")
                raft = entity;
            if (entity->name == "player")
                player = entity;
        }

        for (auto entity : world->getEntities())
        {
            auto enemy = entity->getComponent<EnemyComponent>();
            if (!enemy)
                continue;

            auto health = entity->getComponent<HealthComponent>();
            if (health && health->currentHealth <= 0.0f)
            {
                if (enemy->state != EnemyState::DEAD)
                {
                    enemy->state = EnemyState::DEAD;
                    // Let marine boats and muskets handle their own death/sinking
                    bool hasBoat = entity->getComponent<MarineBoatComponent>() != nullptr;
                    bool hasMusket = entity->getComponent<MusketComponent>() != nullptr;
                    if (!hasBoat && !hasMusket)
                    {
                        world->markForRemoval(entity);
                    }
                }
                // For boats/muskets: still run their update functions to handle sinking/death anim
                // but skip if already being removed (shark etc.)
                if (entity->getComponent<MarineBoatComponent>() == nullptr &&
                    entity->getComponent<MusketComponent>() == nullptr)
                {
                    continue;
                }
            }

            switch (enemy->type)
            {
            case EnemyType::SHARK:
                updateShark(world, entity, enemy, raft, boat, deltaTime);
                break;
            }

            auto boat = entity->getComponent<MarineBoatComponent>();
            if (boat)
            {
                updateMarineBoat(world, entity, boat, raft, deltaTime);
            }
            auto musket = entity->getComponent<MusketComponent>();
            if (musket)
            {
                updateMusket(world, entity, musket, player, deltaTime);
            }
        }
    }

    void CombatSystem::updateShark(World *world, Entity *entity, EnemyComponent *enemy, Entity *raft, Entity *boat, float deltaTime)
    {
        if (!raft)
            return;
        auto shark = entity->getComponent<SharkComponent>();
        if (!shark)
            return;

        if (shark->damageFlashTimer > 0.0f)
            shark->damageFlashTimer -= deltaTime;

        auto animator = entity->getComponent<AnimatorComponent>();

        glm::vec3 sharkPos = entity->localTransform.position;
        glm::vec3 raftPos = raft->localTransform.position;
        float dist = glm::distance(glm::vec3(sharkPos.x, 0.0f, sharkPos.z), glm::vec3(raftPos.x, 0.0f, raftPos.z));

        switch (shark->state)
        {
        case SharkState::APPROACHING:
        {
            if (animator)
                animator->currentAnimIndex = 4; // Swim

            // ── Boat avoidance: if too close to the boat, submerge and respawn ──
            if (boat)
            {
                glm::vec3 boatPos = boat->localTransform.position;
                float boatDist = glm::distance(glm::vec3(sharkPos.x, 0.0f, sharkPos.z), glm::vec3(boatPos.x, 0.0f, boatPos.z));
                if (boatDist < 18.0f) // Avoidance radius
                {
                    shark->state = SharkState::SUBMERGED;
                    shark->stateTimer = 0.0f;
                    if (animator)
                        animator->currentAnimIndex = 4; // Swim anim while submerging
                    break;
                }
            }

            glm::vec3 dir = glm::vec3(raftPos.x, 0.0f, raftPos.z) - glm::vec3(sharkPos.x, 0.0f, sharkPos.z);
            if (glm::length(dir) > 0.0001f)
            {
                dir = glm::normalize(dir);
                entity->localTransform.position += dir * shark->speed * deltaTime;
                entity->localTransform.rotation.y = atan2(-dir.z, dir.x) + glm::radians(90.0f);
                entity->localTransform.rotation.x = 0;
            }
            if (dist < shark->attackRange)
            {
                shark->state = SharkState::ATTACKING;
                if (animator)
                    animator->currentAnimIndex = 0; // Bite
                shark->stateTimer = 0.0f;
                // Play shark attack sound
                playSound("assets/audios/shark.mp3", 1.0f);
            }
            break;
        }
        case SharkState::ATTACKING:
        { // ATTACKING
            float attackAngle = shark->hasAttackedBefore ? -8.0f : -15.0f;
            entity->localTransform.rotation.x = glm::radians(attackAngle);
            shark->stateTimer += deltaTime;
            if (shark->stateTimer >= 0.5f && (shark->stateTimer - deltaTime) < 0.5f)
            {
                auto raftHealth = raft->getComponent<HealthComponent>();
                if (raftHealth)
                {
                    raftHealth->takeDamage(enemy->attackDamage);
                }
            }
            if (shark->stateTimer > 1.5f)
            {
                shark->hasAttackedBefore = true;
                shark->state = SharkState::SUBMERGED;
                shark->stateTimer = 0.0f;
            }
            break;
        }
        case SharkState::SUBMERGED:
        { // SUBMERGED
            entity->localTransform.position.y -= 10.0f * deltaTime;
            shark->stateTimer += deltaTime;
            if (shark->stateTimer > 2.0f)
            {
                float angle = static_cast<float>(rand() % 360) * glm::pi<float>() / 180.0f;
                float spawnDist = 100.0f;
                entity->localTransform.position = glm::vec3(raftPos.x + cos(angle) * spawnDist, -7.0f, raftPos.z + sin(angle) * spawnDist);
                entity->localTransform.rotation.x = 0.0f;
                shark->state = SharkState::APPROACHING;
                shark->stateTimer = 0.0f;
            }
            break;
        }
        case SharkState::DEAD:
        {
            world->markForRemoval(entity);
            break;
        }
        }
    }

    void CombatSystem::updateMarineBoat(World *world, Entity *entity,
                                        MarineBoatComponent *boat,
                                        Entity *raft, float deltaTime)
    {
        if (!raft)
            return;

        auto health = entity->getComponent<HealthComponent>();
        
        // --- Sinking when dead ---
        if (health && health->currentHealth <= 0.0f)
        {
            entity->localTransform.position.y -= 1.5f * deltaTime; // sink slowly
            // After fully submerged, remove boat and its muskets
            if (entity->localTransform.position.y < -5.0f)
            {
                world->markForRemoval(entity);
                // Also mark musket children for removal
                for (auto child : world->getEntities()) {
                    if (child->parent == entity) {
                        world->markForRemoval(child);
                    }
                }
            }
            return; // stop all other behavior while sinking
        }

        // --- Wave physics: float on water ---
        float time = (float)glfwGetTime();
        float bX = entity->localTransform.position.x;
        float bZ = entity->localTransform.position.z;
        float waveH = getWaveHeight(bX, bZ, time) - 2.5f; // deeper submersion, hull underwater
        
        glm::vec3 diff = raft->localTransform.position - entity->localTransform.position;
        diff.y = 0;
        float dist = glm::length(diff);

        if (dist > boat->approachDistance)
        {
            // Beyond approach distance: move toward raft
            glm::vec3 dir = glm::normalize(diff);
            entity->localTransform.position += dir * boat->speed * deltaTime;
            entity->localTransform.position.y = waveH;

            float targetYaw = atan2(dir.x, dir.z);
            entity->localTransform.rotation.y = targetYaw;

            boat->state = MARINE_BOAT_STATE::APPROACHING;
        }
        else
        {
            // Within approach distance: stop moving, bob on waves
            entity->localTransform.position.y = waveH;
            boat->state = MARINE_BOAT_STATE::IDLE;
        }

        // Wave tilting (marine-boat.fbx is already flat, no -90 offset needed)
        glm::vec2 slope = getWaveDx(bX, bZ, time);
        entity->localTransform.rotation.x = std::atan(slope.y) * 0.25f;
        entity->localTransform.rotation.z = -std::atan(slope.x) * 0.25f;
    }

    void CombatSystem::updateMusket(World *world, Entity *entity,
                                    MusketComponent *musket,
                                    Entity *player, float deltaTime)
    {
        if (!player)
            return;

        // Skip if entity is hidden (scale=0) — used by phase system
        if (entity->localTransform.scale.x == 0.0f &&
            entity->localTransform.scale.y == 0.0f &&
            entity->localTransform.scale.z == 0.0f)
            return;

        // ── Approach distance check: only shoot when parent boat is close enough ──
        bool boatApproaching = false;
        if (entity->parent)
        {
            auto *boatComp = entity->parent->getComponent<MarineBoatComponent>();
            if (boatComp && boatComp->state == MARINE_BOAT_STATE::APPROACHING)
            {
                boatApproaching = true;
            }
        }

        auto animator = entity->getComponent<AnimatorComponent>();
        auto health = entity->getComponent<HealthComponent>();
        auto burn = entity->getComponent<BurnComponent>();

        // ── Death animation (index 7) — hold last frame ─────────────────────
        if (health && health->currentHealth <= 0.0f)
        {
            if (animator && animator->currentAnimIndex != 7)
            {
                animator->currentAnimIndex = 7;
                animator->currentAnimationTime = 0.0f;
                animator->loopAnimation = false;
                animator->playSpeed = 1.0f;
            }
            // Don't shoot or rotate when dead
            return;
        }

        // ── Burning: play hurt animation (index 6), stop shooting ──────────
        if (burn && burn->remainingTime > 0.0f)
        {
            if (animator && animator->currentAnimIndex != 6)
            {
                animator->currentAnimIndex = 6;
                animator->currentAnimationTime = 0.0f;
                animator->loopAnimation = true;
                animator->playSpeed = 1.0f;
            }
            // While burning, face player but don't shoot
            musket->state = MusketState::IDLE;
            musket->timer = 0.0f;
            return;
        }
        else
        {
            // Not burning: reset loop flag and return to idle animation
            if (animator)
            {
                animator->loopAnimation = true;
                if (animator->currentAnimIndex == 6)
                {
                    animator->currentAnimIndex = 0;
                    animator->currentAnimationTime = 0.0f;
                    animator->playSpeed = 1.0f;
                }
            }
        }

        musket->timer += deltaTime;

        // ── Compute musket world position (needed for aim direction) ─────────
        glm::vec3 globalPos;
        if (entity->parent)
        {
            glm::mat4 worldMat = entity->parent->getLocalToWorldMatrix() * entity->localTransform.toMat4();
            globalPos = glm::vec3(worldMat[3]);
        }
        else
        {
            globalPos = entity->localTransform.position;
        }

        // ── Face the player ──────────────────────────────────────────────────
        glm::vec3 diff = player->localTransform.position - globalPos;
        if (glm::length(diff) > 0.1f)
        {
            float targetYaw = atan2(diff.x, diff.z);

            // Musket is parented to boat, so subtract parent's world yaw
            if (entity->parent)
                targetYaw -= entity->parent->localTransform.rotation.y;

            float currentYaw = entity->localTransform.rotation.y;
            float yawDiff = targetYaw - currentYaw;

            // Wrap to [-pi, pi]
            while (yawDiff > glm::pi<float>())
                yawDiff -= 2.0f * glm::pi<float>();
            while (yawDiff < -glm::pi<float>())
                yawDiff += 2.0f * glm::pi<float>();

            entity->localTransform.rotation.y += yawDiff * deltaTime * 1.5f;
        }

        // ── State machine ────────────────────────────────────────────────────
        if (musket->state == MusketState::IDLE) // Idle — waiting to shoot
        {
            if (boatApproaching)
            {
                // Boat still approaching — can't shoot, stay idle
                musket->timer = 0.0f;
            }
            else if (musket->timer >= musket->fireRate)
            {
                musket->state = MusketState::FIRING;
                musket->timer = 0.0f;
                if (animator)
                {
                    animator->currentAnimIndex = 17;
                    animator->currentAnimationTime = 0.0f;
                    animator->playSpeed = 0.8f;
                    animator->loopAnimation = true;
                }
            }
        }
        else if (musket->state == MusketState::FIRING) // Shooting — wait for animation to finish
        {
            // If boat moved away, cancel the shot
            if (boatApproaching) {
                musket->state = MusketState::IDLE;
                musket->timer = 0.0f;
                if (animator) {
                    animator->currentAnimIndex = 0;
                    animator->playSpeed = 1.0f;
                    animator->loopAnimation = true;
                }
            }
            else
            {
            float animDuration = 4.0f;
            if (ModelLoader::models.count("marine_musket"))
            {
                auto *m = ModelLoader::models["marine_musket"];
                if (m && m->getScene() && m->getScene()->HasAnimations() && m->getScene()->mNumAnimations > 17)
                {
                    auto *anim = m->getScene()->mAnimations[17];
                    float tps = anim->mTicksPerSecond != 0 ? (float)anim->mTicksPerSecond : 25.0f;
                    float speed = animator ? animator->playSpeed : 1.0f;
                    if (speed < 0.001f) speed = 1.0f;
                    animDuration = ((float)anim->mDuration / tps) / speed - 0.1f;
                }
            }

            if (musket->timer >= animDuration)
            {
                // Animation finished: deal damage and return to IDLE
                if (auto *playerHealth = player->getComponent<HealthComponent>())
                {
                    playerHealth->takeDamage(musket->damage);
                    // Play shotgun sound (lower volume)
                    playSound("assets/audios/shotgun.mp3", 0.4f);
                }

                musket->state = MusketState::IDLE;
                musket->timer = 0.0f;
                if (animator)
                {
                    animator->currentAnimIndex = 0;
                    animator->playSpeed = 1.0f;
                    animator->loopAnimation = true;
                }
            }
            } // end else (not boatApproaching)
        }
    }

}