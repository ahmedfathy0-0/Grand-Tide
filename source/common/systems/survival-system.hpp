#pragma once

#include "../ecs/world.hpp"
#include "../ecs/event-manager.hpp"
#include "../components/health.hpp"
#include "../components/inventory.hpp"
#include "../components/resource.hpp"
#include "../components/mesh-renderer.hpp"
#include "../application.hpp" // Assuming this gives access to Application and inputs
#include <iostream>

namespace our {

    class SurvivalSystem {
        World* world;
        Application* app;
        Entity* playerEntity;
        Entity* boatEntity;

    public:
        void setup(World* world, Application* app, Entity* player, Entity* boat) {
            this->world = world;
            this->app = app;
            this->playerEntity = player;
            this->boatEntity = boat;

            EventManager::subscribe("BOAT_DAMAGED", [this](int damage) {
                if (this->boatEntity) {
                    auto boatHealth = this->boatEntity->getComponent<HealthComponent>();
                    if (boatHealth) {
                        boatHealth->takeDamage(static_cast<float>(damage));
                        std::cout << "[Event] Boat damaged for " << damage << " hp. Current: " << boatHealth->currentHealth << "\n";
                        if (boatHealth->isDead()) {
                            std::cout << "[Event] Boat destroyed!\n";
                            // Swap boat material to a dark/broken material
                            auto meshR = this->boatEntity->getComponent<MeshRendererComponent>();
                            if (meshR) {
                                meshR->material = AssetLoader<Material>::get("dark_broken"); 
                            }
                        }
                    }
                }
            });
        }

        void update();
    };

}