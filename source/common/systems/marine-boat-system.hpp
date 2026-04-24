#pragma once

#include "ecs/world.hpp"
#include "components/marine-boat-component.hpp"
#include "components/musket-component.hpp"
#include "components/mesh-renderer.hpp"
#include "components/health.hpp"
#include "asset-loader.hpp"
#include <glm/glm.hpp>
#include <iostream>

namespace our
{
    class MarineBoatSystem
    {
        float spawnTimer = 10.0f; // Start full so first wave spawns immediately
        int spawnedBoats = 0;
        int maxBoats = 6;

        // Local-space offsets for muskets on the boat.
        // Boat scale is 0.015, so local units are large — these match your original values.
        static constexpr float MUSKET_Y = 100.0f;
        static constexpr float MUSKET_Z_FRONT = 600.0f;
        static constexpr float MUSKET_Z_BACK = -600.0f;
        // Counter-scale to undo boat's 0.015 scale so muskets appear at world size.
        // 1.0 / 0.015 = 66.67
        static constexpr float MUSKET_SCALE = 66.67f;

    public:
        void update(World *world, float deltaTime)
        {
            if (spawnedBoats >= maxBoats)
                return;

            spawnTimer += deltaTime;
            if (spawnTimer < 10.0f)
                return;
            spawnTimer = 0.0f;

            // Spawn 2 boats per wave
            for (int i = 0; i < 2 && spawnedBoats < maxBoats; i++)
            {
                spawnBoat(world, i);
                spawnedBoats++;
            }
        }

    private:
        void spawnBoat(World *world, int waveSlot)
        {
            float startZ = 200.0f + (spawnedBoats * 20.0f);
            float startX = (waveSlot % 2 == 0) ? -35.0f : 35.0f;

            // ── Boat entity ──────────────────────────────────────────────
            Entity *boat = world->add();
            boat->name = "marine_boat_" + std::to_string(spawnedBoats);
            boat->parent = nullptr;

            boat->localTransform.position = glm::vec3(startX, -0.2f, startZ);
            boat->localTransform.scale = glm::vec3(0.015f);
            boat->localTransform.rotation = glm::vec3(0, glm::pi<float>(), 0);

            auto *mr = boat->addComponent<MeshRendererComponent>();
            mr->mesh = nullptr;
            mr->material = AssetLoader<Material>::get("lit_boat");

            auto *boatComp = boat->addComponent<MarineBoatComponent>();
            boatComp->modelName = "marine_boat";
            boatComp->speed = 8.0f;
            boatComp->attackRange = 40.0f;

            auto *health = boat->addComponent<HealthComponent>();
            health->maxHealth = 200;
            health->currentHealth = 200;

            // ── Musket child entities ────────────────────────────────────
            spawnMusket(world, boat, 0);
            spawnMusket(world, boat, 1);

            std::cout << "[MarineBoatSystem] Spawned: " << boat->name << "\n";
        }

        void spawnMusket(World *world, Entity *boat, int slot)
        {
            Entity *musket = world->add();
            musket->parent = boat;
            musket->name = boat->name + "_musket_" + std::to_string(slot);

            float zOffset = (slot == 0) ? MUSKET_Z_FRONT : MUSKET_Z_BACK;
            musket->localTransform.position = glm::vec3(0.0f, MUSKET_Y, zOffset);
            musket->localTransform.scale = glm::vec3(MUSKET_SCALE);
            musket->localTransform.rotation = glm::vec3(0.0f);

            auto *mr = musket->addComponent<MeshRendererComponent>();
            mr->mesh = nullptr;
            mr->material = AssetLoader<Material>::get("lit_musket");

            auto *musketComp = musket->addComponent<MusketComponent>();
            musketComp->modelName = "marine_musket";
            musketComp->musketIndex = slot;
            musketComp->damage = 5.0f;
            musketComp->fireRate = 2.5f;
            musketComp->timer = 0.0f;
            musketComp->state = MusketState::IDLE;
        }
    };
}