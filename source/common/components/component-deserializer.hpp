#pragma once

#include "../ecs/entity.hpp"
#include "camera.hpp"
#include "mesh-renderer.hpp"
#include "free-camera-controller.hpp"
#include "movement.hpp"
#include "health.hpp"
#include "inventory.hpp"
#include "resource.hpp"
#include "light.hpp"
#include "player-controller.hpp"
#include "enemy.hpp"
#include "animator.hpp"
#include "marine-boat-component.hpp"
#include "musket-component.hpp"
#include "shark-component.hpp"
#include "fireball-component.hpp"
#include "projectile-component.hpp"
#include "burn-component.hpp"
namespace our
{

    // Given a json object, this function picks and creates a component in the given entity
    // based on the "type" specified in the json object which is later deserialized from the rest of the json object
    inline void deserializeComponent(const nlohmann::json &data, Entity *entity)
    {
        std::string type = data.value("type", "");
        Component *component = nullptr;
        // TODO: (Req 8) Add an option to deserialize a "MeshRendererComponent" to the following if-else statement
        if (type == CameraComponent::getID())
        {
            component = entity->addComponent<CameraComponent>();
        }
        else if (type == FreeCameraControllerComponent::getID())
        {
            component = entity->addComponent<FreeCameraControllerComponent>();
        }
        else if (type == PlayerControllerComponent::getID())
        {
            component = entity->addComponent<PlayerControllerComponent>();
        }
        else if (type == MovementComponent::getID())
        {
            component = entity->addComponent<MovementComponent>();
        }
        else if (type == MeshRendererComponent::getID())
        {
            component = entity->addComponent<MeshRendererComponent>();
        }
        else if (type == HealthComponent::getID())
        {
            component = entity->addComponent<HealthComponent>();
        }
        else if (type == InventoryComponent::getID())
        {
            component = entity->addComponent<InventoryComponent>();
        }
        else if (type == ResourceComponent::getID())
        {
            component = entity->addComponent<ResourceComponent>();
        }
        else if (type == LightComponent::getID())
        {
            component = entity->addComponent<LightComponent>();
        }
        else if (type == EnemyComponent::getID())
        {
            component = entity->addComponent<EnemyComponent>();
        }
        else if (type == AnimatorComponent::getID())
        {
            component = entity->addComponent<AnimatorComponent>();
        }
        else if (type == MarineBoatComponent::getID())
        {
            component = entity->addComponent<MarineBoatComponent>();
        }
        else if (type == MusketComponent::getID())
        {
            component = entity->addComponent<MusketComponent>();
        }
        else if (type == SharkComponent::getID())
        {
            component = entity->addComponent<SharkComponent>();
        }
        else if (type == FireballComponent::getID())
        {
            component = entity->addComponent<FireballComponent>();
        }
        else if (type == ProjectileComponent::getID())
        {
            component = entity->addComponent<ProjectileComponent>();
        }
        else if (type == BurnComponent::getID())
        {
            component = entity->addComponent<BurnComponent>();
        }
        if (component)
            component->deserialize(data);
    }

}
