#pragma once

#include <ecs/world.hpp>
#include <components/shark-component.hpp>
#include <components/enemy.hpp>
#include <components/health.hpp>
#include <components/mesh-renderer.hpp>
#include <components/animator.hpp>
#include <asset-loader.hpp>
#include <json/json.hpp>
#include <glm/glm.hpp>
#include <iostream>

namespace our
{
    // Phase handler for the SHARKS phase
    class SharkPhase
    {
        nlohmann::json config;
        int sharksToKill = 3;
        int sharksSpawned = 0;
        int sharksKilled = 0;
        float sharkSpawnTimer = 0.0f;
        float sharkSpawnInterval = 8.0f;

    public:
        void enter(World *world, const nlohmann::json &cfg)
        {
            config = cfg;
            sharksSpawned = 0;
            sharksKilled = 0;
            sharkSpawnTimer = 0.0f;

            if (config.contains("sharksToKill"))
                sharksToKill = config["sharksToKill"].get<int>();
            if (config.contains("sharkSpawnInterval"))
                sharkSpawnInterval = config["sharkSpawnInterval"].get<float>();

            std::cout << "[SharkPhase] Enter: " << sharksToKill << " sharks to kill, interval " << sharkSpawnInterval << "s" << std::endl;
        }

        // Returns true when phase is complete (all sharks killed)
        bool update(World *world, float deltaTime)
        {
            // Count killed sharks
            sharksKilled = 0;
            for (auto entity : world->getEntities())
            {
                auto *enemy = entity->getComponent<EnemyComponent>();
                auto *shark = entity->getComponent<SharkComponent>();
                if (shark && enemy && enemy->state == EnemyState::DEAD)
                    sharksKilled++;
            }

            // Spawn sharks until we have enough
            if (sharksSpawned < sharksToKill)
            {
                sharkSpawnTimer += deltaTime;
                if (sharkSpawnTimer >= sharkSpawnInterval)
                {
                    sharkSpawnTimer = 0.0f;
                    spawnShark(world);
                }
            }

            // Phase complete when all sharks are killed
            return (sharksKilled >= sharksToKill && sharksSpawned >= sharksToKill);
        }

        void exit()
        {
            std::cout << "[SharkPhase] Complete: " << sharksKilled << "/" << sharksToKill << " sharks killed" << std::endl;
        }

    private:
        void spawnShark(World *world)
        {
            // Find raft for spawn position
            Entity *raft = nullptr;
            for (auto entity : world->getEntities())
            {
                if (entity->name == "raft")
                {
                    raft = entity;
                    break;
                }
            }
            if (!raft)
                return;

            glm::vec3 raftPos = raft->localTransform.position;
            float angle = static_cast<float>(rand() % 360) * glm::pi<float>() / 180.0f;

            // Get shark config from JSON (cycle through sharks array)
            nlohmann::json sharkData;
            if (config.contains("sharks") && config["sharks"].is_array() && config["sharks"].size() > 0)
            {
                int idx = sharksSpawned % (int)config["sharks"].size();
                sharkData = config["sharks"][idx];
            }

            float spawnDist = sharkData.value("spawnDistance", 150.0f) + static_cast<float>(rand() % 100);
            float spawnY = sharkData.value("spawnY", -10.0f);
            float health = sharkData.value("health", 100.0f);
            float speed = sharkData.value("speed", 12.0f);
            float attackRange = sharkData.value("attackRange", 25.0f);
            float attackDamage = sharkData.value("attackDamage", 100.0f);
            std::string material = sharkData.value("material", "lit_shark");
            std::string model = sharkData.value("model", "shark");
            int animIndex = sharkData.value("animIndex", 0);

            Entity *shark = world->add();
            shark->name = "shark_" + std::to_string(sharksSpawned);
            shark->localTransform.position = glm::vec3(
                raftPos.x + cos(angle) * spawnDist,
                spawnY,
                raftPos.z + sin(angle) * spawnDist);

            // Scale from config or default
            if (sharkData.contains("scale") && sharkData["scale"].is_array())
            {
                shark->localTransform.scale = glm::vec3(
                    sharkData["scale"][0].get<float>(),
                    sharkData["scale"][1].get<float>(),
                    sharkData["scale"][2].get<float>());
            }
            else
            {
                shark->localTransform.scale = glm::vec3(0.05f);
            }
            shark->localTransform.rotation = glm::vec3(0.0f);

            auto *mr = shark->addComponent<MeshRendererComponent>();
            mr->mesh = nullptr;
            mr->material = AssetLoader<Material>::get(material);

            auto *sc = shark->addComponent<SharkComponent>();
            sc->health = health;
            sc->state = SharkState::APPROACHING;
            sc->speed = speed;
            sc->attackRange = attackRange;

            auto *anim = shark->addComponent<AnimatorComponent>();
            anim->modelName = model;
            anim->currentAnimIndex = animIndex;

            auto *enemy = shark->addComponent<EnemyComponent>();
            enemy->type = EnemyType::SHARK;
            std::string targetStr = sharkData.value("primaryTarget", "RAFT");
            enemy->primaryTarget = (targetStr == "PLAYER") ? PrimaryTarget::PLAYER : PrimaryTarget::RAFT;
            enemy->state = EnemyState::ALIVE;
            enemy->attackDamage = attackDamage;

            auto *hp = shark->addComponent<HealthComponent>();
            hp->maxHealth = health;
            hp->currentHealth = health;

            sharksSpawned++;
        }
    };
}
