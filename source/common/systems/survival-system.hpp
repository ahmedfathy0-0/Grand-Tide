#pragma once

#include "../ecs/world.hpp"
#include "../ecs/event-manager.hpp"
#include "../components/health.hpp"
#include "../components/inventory.hpp"
#include "../components/resource.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/shark-component.hpp"
#include "../application.hpp"
#include <miniaudio.h>
#include <iostream>

namespace our {

    class SurvivalSystem {
        World* world;
        Application* app;
        Entity* playerEntity;
        Entity* boatEntity;
        bool eventsSubscribed = false;

        // Pain sound
        ma_engine* painSoundEngine = nullptr;

        void handleBoatDamaged(int damage);
        void handleRepairAction();
        void handleGatherAction();
        void handleAttackAction();

    public:
        void setup(World* world, Application* app, Entity* player, Entity* boat) {
            this->world = world;
            this->app = app;
            this->playerEntity = player;
            this->boatEntity = boat;

            if(!eventsSubscribed) {
                EventManager::subscribe("BOAT_DAMAGED", [this](int damage) { handleBoatDamaged(damage); });
                EventManager::subscribe("PLAYER_REPAIR_ACTION", [this](int) { handleRepairAction(); });
                EventManager::subscribe("PLAYER_GATHER_ACTION", [this](int) { handleGatherAction(); });
                EventManager::subscribe("PLAYER_ATTACK_ACTION", [this](int) { handleAttackAction(); });
                eventsSubscribed = true;
            }
        }

        void update();

    private:
        void playPainSound();
    };

}