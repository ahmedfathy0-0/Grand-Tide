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
            std::string model = octCfg.value("model", "octopus");
            float health = octCfg.value("health", 500.0f);
            float attackDamage = octCfg.value("attackDamage", 10.0f);
            float attackCooldown = octCfg.value("attackCooldown", 5.0f);
            float idleDuration = octCfg.value("idleDuration", 5.0f);
            float attackDuration = octCfg.value("attackDuration", 0.0f);
            float emergeDelay = octCfg.value("emergeDelay", 10.0f);
            float attackRange = octCfg.value("attackRange", 35.0f);
            float chaseSpeed = octCfg.value("chaseSpeed", 8.0f);
            float surfacedY = octCfg.value("surfacedY", -5.0f);
            float submergedY = octCfg.value("submergedY", -150.0f);
            float followDistance = octCfg.value("followDistance", 60.0f);
            float minFollowDist = octCfg.value("minFollowDistance", 30.0f);
            float maxFollowDist = octCfg.value("maxFollowDistance", 100.0f);
            float speed = octCfg.value("speed", 1.0f);

            Entity *octopus = world->add();
            octopus->name = "octopus";
            octopus->localTransform.position = pos;
            octopus->localTransform.scale = scale;
            octopus->localTransform.rotation = rotation;

            auto *mr = octopus->addComponent<MeshRendererComponent>();
            mr->mesh = nullptr;
            mr->material = AssetLoader<Material>::get(material);

            auto *octComp = octopus->addComponent<OctopusComponent>();
            octComp->modelName = model;
            octComp->health = health;
            octComp->attackDamage = attackDamage;
            octComp->attackCooldown = attackCooldown;
            octComp->idleDuration = idleDuration;
            octComp->attackDuration = attackDuration;
            octComp->emergeDelay = emergeDelay;
            octComp->attackRange = attackRange;
            octComp->chaseSpeed = chaseSpeed;
            octComp->surfacedY = surfacedY;
            octComp->submergedY = submergedY;
            octComp->followDistance = followDistance;
            octComp->minFollowDistance = minFollowDist;
            octComp->maxFollowDistance = maxFollowDist;
            octComp->speed = speed;

            // Read additional fields that were previously left at defaults
            octComp->tentacleRadius = octCfg.value("tentacleRadius", octComp->tentacleRadius);
            octComp->playerRadius = octCfg.value("playerRadius", octComp->playerRadius);
            octComp->hitCooldownDuration = octCfg.value("hitCooldownDuration", octComp->hitCooldownDuration);
            octComp->stunDuration = octCfg.value("stunDuration", octComp->stunDuration);
            octComp->revivalHP = octCfg.value("revivalHP", octComp->revivalHP);
            octComp->damageMultiplier = octCfg.value("damageMultiplier", octComp->damageMultiplier);
            octComp->waveMaxRadius = octCfg.value("waveMaxRadius", octComp->waveMaxRadius);
            octComp->waveExpandSpeed = octCfg.value("waveExpandSpeed", octComp->waveExpandSpeed);
            octComp->waveDamage = octCfg.value("waveDamage", octComp->waveDamage);

            auto *anim = octopus->addComponent<AnimatorComponent>();
            anim->modelName = model;
            anim->currentAnimIndex = 0;

            auto *hp = octopus->addComponent<HealthComponent>();
            hp->maxHealth = health;
            hp->currentHealth = health;

            octopusEntity = octopus;
            std::cout << "[KrakenPhase] Spawned octopus (HP=" << health << ")" << std::endl;
        }
    };
}
