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
    void CombatSystem::update(World *world, float deltaTime)
    {
        Entity *raft = nullptr;
        Entity *player = nullptr;

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
                    world->markForRemoval(entity);
                }
                continue;
            }

            switch (enemy->type)
            {
            case EnemyType::SHARK:
                updateShark(world, entity, enemy, raft, deltaTime);
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

    void CombatSystem::updateShark(World *world, Entity *entity, EnemyComponent *enemy, Entity *raft, float deltaTime)
    {
        if (!raft)
            return;
        auto shark = entity->getComponent<SharkComponent>();
        if (!shark)
            return;
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
            }
            break;
        }
        case SharkState::ATTACKING:
        { // ATTACKING
            entity->localTransform.rotation.x = glm::radians(-15.0f);
            shark->stateTimer += deltaTime;
            if (shark->stateTimer >= 0.5f && (shark->stateTimer - deltaTime) < 0.5f)
            {
                auto raftHealth = raft->getComponent<HealthComponent>();
                if (raftHealth)
                {
                    raftHealth->takeDamage(enemy->attackDamage);
                    std::cout << "Shark attacked! Dealt " << enemy->attackDamage << " damage." << std::endl;
                }
            }
            if (shark->stateTimer > 1.5f)
            {
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

        glm::vec3 diff = raft->localTransform.position - entity->localTransform.position;
        diff.y = 0;
        float distSq = glm::length2(diff);

        if (distSq > boat->attackRange * boat->attackRange)
        {
            glm::vec3 dir = glm::normalize(diff);
            entity->localTransform.position += dir * boat->speed * deltaTime;

            float targetYaw = atan2(dir.x, dir.z);
            entity->localTransform.rotation.y = targetYaw;
        }
    }

    void CombatSystem::updateMusket(World *world, Entity *entity,
                                    MusketComponent *musket,
                                    Entity *player, float deltaTime)
    {
        if (!player)
            return;

        auto animator = entity->getComponent<AnimatorComponent>();

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
            // so the rotation stays correct in local space
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
            if (musket->timer >= musket->fireRate) // User requested > 5 sec
            {
                musket->state = MusketState::FIRING; // Enter shooting state
                musket->timer = 0.0f;
                if (animator)
                {
                    animator->currentAnimIndex = 17;
                    animator->currentAnimationTime = 0.0f;
                    animator->playSpeed = 0.8f; // Slower shooting
                }
            }
        }
        else if (musket->state == MusketState::FIRING) // Shooting — wait for animation to finish
        {
            float animDuration = 4.0f; // Adjusted fallback for slower speed
            if (ModelLoader::models.count("marine_musket"))
            {
                auto *m = ModelLoader::models["marine_musket"];
                if (m && m->getScene() && m->getScene()->HasAnimations() && m->getScene()->mNumAnimations > 17)
                {
                    auto *anim = m->getScene()->mAnimations[17];
                    float tps = anim->mTicksPerSecond != 0 ? (float)anim->mTicksPerSecond : 25.0f;
                    float speed = animator ? animator->playSpeed : 1.0f;
                    if (speed < 0.001f) speed = 1.0f;
                    animDuration = ((float)anim->mDuration / tps) / speed - 0.1f; // Subtract epsilon to prevent loop pop
                }
            }

            if (musket->timer >= animDuration)
            {
                // Animation finished: deal damage and return to IDLE
                if (auto *health = player->getComponent<HealthComponent>())
                {
                    health->takeDamage(musket->damage);
                    std::cout << "[Musket] musket " << musket->musketIndex+musket->boatIndex << "     Fire! Dealt " << musket->damage << " damage.\n";
                }

                musket->state = MusketState::IDLE;
                musket->timer = 0.0f;
                if (animator)
                {
                    animator->currentAnimIndex = 0;
                    animator->playSpeed = 1.0f; // Normal speed for idle
                }
            }
        }
    }

}