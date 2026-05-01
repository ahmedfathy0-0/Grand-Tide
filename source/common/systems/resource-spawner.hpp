#pragma once

#include "../ecs/world.hpp"
#include "../components/resource.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/animator.hpp"
#include "../asset-loader.hpp"
#include <glm/glm.hpp>
#include <iostream>
#include <string>

namespace our {

    // Handles resource (fish/wood) spawning and lifetime expiration.
    class ResourceSpawner {
        int fishSpawned = 0;
        int woodSpawned = 0;
        float fishSpawnTimer = 0.0f;
        float woodSpawnTimer = 0.0f;
        float fishSpawnInterval = 5.0f;
        float woodSpawnInterval = 5.0f;
        int maxFishAlive = 5;
        int maxWoodAlive = 5;

    public:
        void update(World* world, float deltaTime) {
            int fishAlive = 0;
            int woodAlive = 0;
            Entity* raft = nullptr;

            for (auto entity : world->getEntities()) {
                if (entity->name == "raft") raft = entity;
                auto res = entity->getComponent<ResourceComponent>();
                if (!res) continue;
                res->elapsed += deltaTime;
                if (res->elapsed >= res->lifetime) {
                    std::cout << "[Expire] " << res->type << " expired at ("
                              << entity->localTransform.position.x << ","
                              << entity->localTransform.position.z << ")" << std::endl;
                    world->markForRemoval(entity);
                    continue;
                }
                if (res->type == "fish") fishAlive++;
                else if (res->type == "wood") woodAlive++;
            }

            fishSpawnTimer += deltaTime;
            woodSpawnTimer += deltaTime;

            if (raft && fishAlive < maxFishAlive && fishSpawnTimer >= fishSpawnInterval) {
                fishSpawnTimer = 0.0f;
                fishSpawnInterval = 3.0f + static_cast<float>(rand() % 3);
                spawnFish(world, raft);
            }
            if (raft && woodAlive < maxWoodAlive && woodSpawnTimer >= woodSpawnInterval) {
                woodSpawnTimer = 0.0f;
                woodSpawnInterval = 6.0f + static_cast<float>(rand() % 4);
                spawnWood(world, raft);
            }
        }

    private:
        void spawnFish(World* world, Entity* raft) {
            glm::vec3 raftPos = raft->localTransform.position;
            float angle = static_cast<float>(rand() % 360) * glm::pi<float>() / 180.0f;
            float spawnDist = 3.0f + static_cast<float>(rand() % 12);

            Entity* fish = world->add();
            fish->name = "fish_" + std::to_string(fishSpawned);
            fish->localTransform.position = glm::vec3(
                raftPos.x + cos(angle) * spawnDist, -1.0f, raftPos.z + sin(angle) * spawnDist);
            fish->localTransform.scale = glm::vec3(0.1f);
            fish->localTransform.rotation = glm::vec3(0.0f);

            auto* mr = fish->addComponent<MeshRendererComponent>();
            mr->mesh = AssetLoader<Mesh>::get("cube");
            mr->material = AssetLoader<Material>::get("lit_fish");

            auto* anim = fish->addComponent<AnimatorComponent>();
            anim->modelName = "fish";
            anim->currentAnimIndex = 0;

            auto* res = fish->addComponent<ResourceComponent>();
            res->type = "fish"; res->amount = 1; res->lifetime = 60.0f; res->elapsed = 0.0f;

            fishSpawned++;
        }

        void spawnWood(World* world, Entity* raft) {
            glm::vec3 raftPos = raft->localTransform.position;
            float angle = static_cast<float>(rand() % 360) * glm::pi<float>() / 180.0f;
            float spawnDist = 3.0f + static_cast<float>(rand() % 12);

            Entity* wood = world->add();
            wood->name = "wood_" + std::to_string(woodSpawned);
            wood->localTransform.position = glm::vec3(
                raftPos.x + cos(angle) * spawnDist, -1.0f, raftPos.z + sin(angle) * spawnDist);
            wood->localTransform.scale = glm::vec3(0.05f);
            wood->localTransform.rotation = glm::vec3(0.0f);

            auto* mr = wood->addComponent<MeshRendererComponent>();
            mr->mesh = AssetLoader<Mesh>::get("wood");
            mr->material = AssetLoader<Material>::get("lit_wood_resource");

            auto* res = wood->addComponent<ResourceComponent>();
            res->type = "wood"; res->amount = 1; res->lifetime = 60.0f; res->elapsed = 0.0f;

            woodSpawned++;
        }
    };

} // namespace our
