#pragma once

#include "ecs/world.hpp"
#include "components/marine-boat-component.hpp"
#include "components/musket-component.hpp"
#include "components/mesh-renderer.hpp"
#include "components/health.hpp"
#include "components/animator.hpp"
#include "components/enemy.hpp"
#include "asset-loader.hpp"
#include <glm/glm.hpp>
#include <iostream>

namespace our
{
    class MarineBoatSystem
    {
        float spawnTimer = 10.0f; // Start full so first wave spawns immediately
        int spawnedBoats = 0;
        int maxBoats = 3;

    public:
        void update(World *world, float deltaTime)
        {
            if (spawnedBoats >= maxBoats)
                return;

            spawnTimer += deltaTime;
            if (spawnTimer < 10.0f)
                return;
            spawnTimer = 0.0f;

            // Spawn up to 2 boats per wave
            for (int i = 0; i < 2 && spawnedBoats < maxBoats; i++)
            {
                spawnBoat(world, i);
                spawnedBoats++;
            }
        }

    private:
        void spawnBoat(World *world, int waveSlot)
        {
            // Spawn far enough that boats must approach (attackRange=40, raft at Z~10)
            float startZ = 80.0f + (spawnedBoats * 30.0f);
            float startX = (waveSlot % 2 == 0) ? -40.0f : 40.0f;

            // ── Boat entity ──────────────────────────────────────────────
            Entity *boat = world->add();
            boat->name = "marine_boat_" + std::to_string(spawnedBoats);
            boat->parent = nullptr;

            // Match debug_marine_boat in app.json: scale 0.05, Y=0, rotation Y=45
            boat->localTransform.position = glm::vec3(startX, 0.0f, startZ);
            boat->localTransform.scale = glm::vec3(0.05f);
            boat->localTransform.rotation = glm::vec3(0.0f, glm::radians(45.0f), 0.0f);

            auto *mr = boat->addComponent<MeshRendererComponent>();
            mr->mesh = nullptr;
            mr->material = AssetLoader<Material>::get("lit_boat");

            auto *boatComp = boat->addComponent<MarineBoatComponent>();
            boatComp->modelName = "marine_boat";
            boatComp->speed = 2.0f;
            boatComp->attackRange = 40.0f;

            auto *boatAnim = boat->addComponent<AnimatorComponent>();
            boatAnim->modelName = "marine_boat";
            boatAnim->currentAnimIndex = 0;

            auto *boatEnemy = boat->addComponent<EnemyComponent>();
            boatEnemy->type = EnemyType::MARINE_BOAT;
            boatEnemy->primaryTarget = PrimaryTarget::RAFT;
            boatEnemy->state = EnemyState::ALIVE;
            boatEnemy->attackDamage = 0.0f;

            auto *health = boat->addComponent<HealthComponent>();
            health->maxHealth = 200;
            health->currentHealth = 200;

            // ── Musket child entities ────────────────────────────────────
            // Match debug_marine_boat children from app.json exactly
            spawnMusket(world, boat, 0,
                        glm::vec3(20.0f, 10.0f, -50.0f),
                        glm::vec3(50.0f, 50.0f, 50.0f));
            spawnMusket(world, boat, 1,
                        glm::vec3(-20.0f, 10.0f, 50.0f),
                        glm::vec3(100.0f, 50.0f, 50.0f));

            std::cout << "[MarineBoatSystem] Spawned: " << boat->name
                      << " at (" << startX << ", 0, " << startZ << ")\n";
        }

        void spawnMusket(World *world, Entity *boat, int slot,
                         const glm::vec3 &localPos,
                         const glm::vec3 &localScale)
        {
            Entity *musket = world->add();
            musket->parent = boat;
            musket->name = boat->name + "_musket_" + std::to_string(slot);

            musket->localTransform.position = localPos;
            musket->localTransform.scale = localScale;
            musket->localTransform.rotation = glm::vec3(0.0f);

            auto *mr = musket->addComponent<MeshRendererComponent>();
            mr->mesh = nullptr;
            mr->material = AssetLoader<Material>::get("lit_musket");

            auto *musketComp = musket->addComponent<MusketComponent>();
            musketComp->modelName = "marine_musket";
            musketComp->musketIndex = slot;
            musketComp->damage = 10.0f;
            musketComp->fireRate = 10.0f;
            musketComp->timer = 0.0f;
            musketComp->state = MusketState::IDLE;

            auto *musketHealth = musket->addComponent<HealthComponent>();
            musketHealth->maxHealth = 100;
            musketHealth->currentHealth = 100;

            auto *musketAnim = musket->addComponent<AnimatorComponent>();
            musketAnim->modelName = "marine_musket";
            musketAnim->currentAnimIndex = 0;

            auto *musketEnemy = musket->addComponent<EnemyComponent>();
            musketEnemy->type = EnemyType::MUSKET;
            musketEnemy->primaryTarget = PrimaryTarget::PLAYER;
            musketEnemy->state = EnemyState::ALIVE;
            musketEnemy->attackDamage = 10.0f;
        }
    };
}