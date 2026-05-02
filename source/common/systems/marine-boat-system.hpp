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
#include <json/json.hpp>
#include <iostream>

namespace our
{
    class MarineBoatSystem
    {
        float spawnTimer = 10.0f;
        int spawnedBoats = 0;
        int totalBoats = 9;
        int maxConcurrent = 4;
        float spawnInterval = 10.0f;
        nlohmann::json config;

    public:
        void setConfig(const nlohmann::json& cfg) {
            config = cfg;
            if (config.contains("number"))
                totalBoats = config["number"].get<int>();
            if (config.contains("maxConcurrent"))
                maxConcurrent = config["maxConcurrent"].get<int>();
            if (config.contains("spawnInterval"))
                spawnInterval = config["spawnInterval"].get<float>();
        }

        void update(World *world, float deltaTime)
        {
            if (spawnedBoats >= totalBoats)
                return;

            // Count currently alive boats
            int aliveBoats = 0;
            for (auto entity : world->getEntities()) {
                auto boat = entity->getComponent<MarineBoatComponent>();
                auto enemy = entity->getComponent<EnemyComponent>();
                if (boat && enemy && enemy->state != EnemyState::DEAD)
                    aliveBoats++;
            }

            // Don't spawn if max concurrent are alive
            if (aliveBoats >= maxConcurrent)
                return;

            spawnTimer += deltaTime;
            if (spawnTimer < spawnInterval)
                return;
            spawnTimer = 0.0f;

            // Spawn one boat at a time (respecting maxConcurrent)
            spawnBoat(world);
            spawnedBoats++;
        }

    private:
        void spawnBoat(World *world)
        {
            nlohmann::json boatCfg = config.value("boat", nlohmann::json::object());

            float startZBase = boatCfg.value("startZ", 80.0f);
            float startZInc = boatCfg.value("startZIncrement", 30.0f);
            float startZ = startZBase + (spawnedBoats * startZInc);

            float startX;
            if (boatCfg.contains("startX") && boatCfg["startX"].is_array() && boatCfg["startX"].size() > 0) {
                int idx = spawnedBoats % (int)boatCfg["startX"].size();
                startX = boatCfg["startX"][idx].get<float>();
            } else {
                startX = (spawnedBoats % 2 == 0) ? -40.0f : 40.0f;
            }

            float rotY = boatCfg.value("rotationY", 45.0f);
            float boatScale = boatCfg.contains("scale") && boatCfg["scale"].is_array()
                ? boatCfg["scale"][0].get<float>() : 0.05f;

            Entity *boat = world->add();
            boat->name = "marine_boat_" + std::to_string(spawnedBoats);
            boat->parent = nullptr;
            boat->localTransform.position = glm::vec3(startX, 0.0f, startZ);
            boat->localTransform.scale = glm::vec3(boatScale);
            boat->localTransform.rotation = glm::vec3(0.0f, glm::radians(rotY), 0.0f);

            auto *mr = boat->addComponent<MeshRendererComponent>();
            mr->mesh = nullptr;
            mr->material = AssetLoader<Material>::get(boatCfg.value("material", "lit_boat"));

            auto *boatComp = boat->addComponent<MarineBoatComponent>();
            boatComp->deserialize(boatCfg);

            auto *boatAnim = boat->addComponent<AnimatorComponent>();
            boatAnim->deserialize(boatCfg);

            auto *boatEnemy = boat->addComponent<EnemyComponent>();
            boatEnemy->deserialize(boatCfg);
            boatEnemy->type = EnemyType::MARINE_BOAT;
            boatEnemy->state = EnemyState::ALIVE;

            auto *health = boat->addComponent<HealthComponent>();
            health->deserialize(boatCfg);

            // Spawn muskets using the single template
            nlohmann::json musketCfg = config.value("musket", nlohmann::json::object());
            if (musketCfg.contains("localPositions") && musketCfg["localPositions"].is_array()) {
                for (size_t i = 0; i < musketCfg["localPositions"].size(); i++) {
                    glm::vec3 localPos(20.0f, 10.0f, -50.0f);
                    glm::vec3 localScale(50.0f, 50.0f, 50.0f);

                    auto& posArr = musketCfg["localPositions"][i];
                    if (posArr.is_array() && posArr.size() >= 3)
                        localPos = glm::vec3(posArr[0].get<float>(), posArr[1].get<float>(), posArr[2].get<float>());

                    if (musketCfg.contains("localScales") && musketCfg["localScales"].is_array() && i < musketCfg["localScales"].size()) {
                        auto& scaleArr = musketCfg["localScales"][i];
                        if (scaleArr.is_array() && scaleArr.size() >= 3)
                            localScale = glm::vec3(scaleArr[0].get<float>(), scaleArr[1].get<float>(), scaleArr[2].get<float>());
                    }

                    spawnMusket(world, boat, (int)i, localPos, localScale, musketCfg);
                }
            } else {
                spawnMusket(world, boat, 0, glm::vec3(20.0f, 10.0f, -50.0f), glm::vec3(50.0f, 50.0f, 50.0f), musketCfg);
                spawnMusket(world, boat, 1, glm::vec3(-20.0f, 10.0f, 50.0f), glm::vec3(100.0f, 50.0f, 50.0f), musketCfg);
            }

            std::cout << "[MarineBoatSystem] Spawned boat #" << spawnedBoats << std::endl;
        }

        void spawnMusket(World *world, Entity *boat, int slot,
                         const glm::vec3 &localPos,
                         const glm::vec3 &localScale,
                         const nlohmann::json& mCfg)
        {
            Entity *musket = world->add();
            musket->parent = boat;
            musket->name = boat->name + "_musket_" + std::to_string(slot);
            musket->localTransform.position = localPos;
            musket->localTransform.scale = localScale;
            musket->localTransform.rotation = glm::vec3(0.0f);

            auto *mr = musket->addComponent<MeshRendererComponent>();
            mr->mesh = nullptr;
            mr->material = AssetLoader<Material>::get(mCfg.value("material", "lit_musket"));

            auto *musketComp = musket->addComponent<MusketComponent>();
            musketComp->deserialize(mCfg);
            musketComp->musketIndex = slot;
            musketComp->timer = 0.0f;
            musketComp->state = MusketState::IDLE;

            auto *musketHealthComp = musket->addComponent<HealthComponent>();
            musketHealthComp->deserialize(mCfg);

            auto *musketAnim = musket->addComponent<AnimatorComponent>();
            musketAnim->deserialize(mCfg);

            auto *musketEnemy = musket->addComponent<EnemyComponent>();
            musketEnemy->deserialize(mCfg);
            musketEnemy->type = EnemyType::MUSKET;
            musketEnemy->state = EnemyState::ALIVE;
        }
    };
}