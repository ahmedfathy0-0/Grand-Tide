#pragma once

#include <ecs/world.hpp>
#include <systems/marine-boat-system.hpp>
#include <components/marine-boat-component.hpp>
#include <components/musket-component.hpp>
#include <components/enemy.hpp>
#include <json/json.hpp>
#include <glm/glm.hpp>
#include <iostream>

namespace our
{
    // Phase handler for the MARINES phase
    class MarinePhase
    {
        nlohmann::json config;
        MarineBoatSystem boatSystem;
        float duration = 300.0f;
        float phaseTimer = 0.0f;

    public:
        void enter(World *world, const nlohmann::json &cfg)
        {
            config = cfg;
            phaseTimer = 0.0f;

            if (config.contains("duration"))
                duration = config["duration"].get<float>();

            boatSystem.setConfig(config);
            std::cout << "[MarinePhase] Enter: duration " << duration << "s" << std::endl;
        }

        // Returns true when phase is complete (survived duration OR all marines dead)
        bool update(World *world, float deltaTime)
        {
            phaseTimer += deltaTime;

            // Run boat spawning system
            boatSystem.update(world, deltaTime);

            // Check if all marines are dead (only if at least one boat has spawned)
            bool anyMarineAlive = false;
            bool anyMarineExists = false;
            for (auto entity : world->getEntities())
            {
                auto boat = entity->getComponent<MarineBoatComponent>();
                auto musket = entity->getComponent<MusketComponent>();
                auto enemy = entity->getComponent<EnemyComponent>();
                if (boat || musket)
                    anyMarineExists = true;
                if ((boat || musket) && enemy && enemy->state != EnemyState::DEAD)
                    anyMarineAlive = true;
            }

            // Phase complete: survived duration, OR all marines dead (but only if marines have spawned)
            bool complete = (phaseTimer >= duration || (anyMarineExists && !anyMarineAlive));
            return complete;
        }

        float getRemainingTime() const
        {
            float remaining = duration - phaseTimer;
            return remaining < 0.0f ? 0.0f : remaining;
        }

        void exit()
        {
            std::cout << "[MarinePhase] Complete: survived " << (int)phaseTimer << "s" << std::endl;
        }
    };
}
