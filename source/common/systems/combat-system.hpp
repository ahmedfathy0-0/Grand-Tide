#pragma once

#include "ecs/world.hpp"
#include "components/enemy.hpp"
#include "components/health.hpp"
#include "components/animator.hpp"
#include "components/mesh-renderer.hpp"
#include "components/musket-component.hpp"
#include "components/marine-boat-component.hpp"
#include "ecs/transform.hpp"
#include "asset-loader.hpp"
#include "mesh/model.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <iostream>

namespace our
{
    class CombatSystem
    {
    public:
        void update(World *world, float deltaTime);

    private:
        void updateShark(World *world, Entity *entity, EnemyComponent *enemy, Entity *raft, float deltaTime);
        void updateMarineBoat(World *world, Entity *entity, MarineBoatComponent *boat, Entity *raft, float deltaTime);
        void updateMusket(World *world, Entity *entity, MusketComponent *musket, Entity *player, float deltaTime);
    };
}