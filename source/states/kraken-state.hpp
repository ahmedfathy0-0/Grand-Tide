#pragma once

#include <ecs/world.hpp>
#include <systems/octopus-system.hpp>
#include <systems/boss-health-bar.hpp>
#include <components/octopus-component.hpp>
#include <components/health.hpp>
#include <components/mesh-renderer.hpp>
#include <components/animator.hpp>
#include <components/enemy.hpp>
#include <asset-loader.hpp>
#include <json/json.hpp>
#include <glm/glm.hpp>
#include <iostream>

namespace our
{
    // Phase handler for the KRAKEN (octopus boss) phase
    class KrakenPhase
    {
        nlohmann::json config;
        OctopusSystem octopusSystem;
        Entity *octopusEntity = nullptr;

    public:
        void enter(World *world, const nlohmann::json &cfg)
        {
            config = cfg;
            spawnOctopus(world);
        }

        // Returns true when octopus is permanently dead
        bool update(World *world, float deltaTime, Application *app)
        {
            // Run octopus AI
            octopusSystem.update(world, deltaTime, app);

            // Check if octopus is permanently dead
            if (octopusEntity) {
                auto *oct = octopusEntity->getComponent<OctopusComponent>();
                if (oct && oct->permanentlyDead)
                    return true;
            }
            return false;
        }

        void onGui(World *world, BossHealthBar &bossHealthBar, float deltaTime)
        {
            // Draw boss health bar
            if (octopusEntity) {
                auto *octopusHealth = octopusEntity->getComponent<HealthComponent>();
                auto *octopusComp = octopusEntity->getComponent<OctopusComponent>();
                if (octopusHealth && octopusComp && !octopusComp->permanentlyDead)
                {
                    bossHealthBar.setRevived(octopusComp->hasRevived);
                    bossHealthBar.update(octopusHealth->currentHealth, octopusHealth->maxHealth, deltaTime);
                    bossHealthBar.render();
                }
            }
        }

        void exit()
        {
            std::cout << "[KrakenPhase] Exit: kraken defeated" << std::endl;
        }

    private:
        void spawnOctopus(World *world)
        {
            nlohmann::json octCfg = config.value("octopus", nlohmann::json::object());

            glm::vec3 pos(80.0f, -150.0f, 10.0f);
            if (octCfg.contains("position") && octCfg["position"].is_array())
                pos = glm::vec3(octCfg["position"][0].get<float>(), octCfg["position"][1].get<float>(), octCfg["position"][2].get<float>());

            glm::vec3 scale(0.08f);
            if (octCfg.contains("scale") && octCfg["scale"].is_array())
                scale = glm::vec3(octCfg["scale"][0].get<float>(), octCfg["scale"][1].get<float>(), octCfg["scale"][2].get<float>());

            glm::vec3 rotation(0.0f);
            if (octCfg.contains("rotation") && octCfg["rotation"].is_array())
                rotation = glm::vec3(octCfg["rotation"][0].get<float>(), octCfg["rotation"][1].get<float>(), octCfg["rotation"][2].get<float>());

            std::string material = octCfg.value("material", "lit_octopus");

            Entity *octopus = world->add();
            octopus->name = "octopus";
            octopus->localTransform.position = pos;
            octopus->localTransform.scale = scale;
            octopus->localTransform.rotation = rotation;

            auto *mr = octopus->addComponent<MeshRendererComponent>();
            mr->mesh = nullptr;
            mr->material = AssetLoader<Material>::get(material);

            auto *octComp = octopus->addComponent<OctopusComponent>();
            octComp->deserialize(octCfg);

            auto *anim = octopus->addComponent<AnimatorComponent>();
            anim->deserialize(octCfg);

            auto *hp = octopus->addComponent<HealthComponent>();
            hp->deserialize(octCfg);

            octopusEntity = octopus;
            std::cout << "[KrakenPhase] Spawned octopus (HP=" << hp->maxHealth << ")" << std::endl;
        }
    };
}
