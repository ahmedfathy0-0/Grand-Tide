#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/player-controller-system.hpp>
#include <systems/movement.hpp>
#include <systems/survival-system.hpp>
#include <systems/animation-system.hpp>
#include <systems/combat-system.hpp>
#include <systems/marine-boat-system.hpp>
#include <systems/fireball-system.hpp>

#include <asset-loader.hpp>

// This state shows how to use the ECS framework and deserialization.
class Playstate : public our::State
{

    our::World world;
    our::ForwardRenderer renderer;
    our::PlayerControllerSystem cameraController; // Now utilizing Player Controller explicitly
    our::MovementSystem movementSystem;
    our::SurvivalSystem survivalSystem;
    our::AnimationSystem animationSystem;
    our::CombatSystem combatSystem;
    our::MarineBoatSystem marineBoatSystem;
    our::FireballSystem fireballSystem;

    void
    onInitialize() override
    {
        // First of all, we get the scene configuration from the app config
        auto &config = getApp()->getConfig()["scene"];
        // If we have assets in the scene config, we deserialize them
        if (config.contains("assets"))
        {
            our::deserializeAllAssets(config["assets"]);
        }
        // If we have a world in the scene config, we use it to populate our world
        if (config.contains("world"))
        {
            world.deserialize(config["world"]);
        }
        // We initialize the camera controller system since it needs a pointer to the app
        cameraController.enter(getApp());
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);

        our::Entity *player = nullptr;
        our::Entity *boat = nullptr;
        for (auto entity : world.getEntities())
        {
            if (entity->name == "player")
                player = entity;
            if (entity->name == "raft")
                boat = entity;
        }
        survivalSystem.setup(&world, getApp(), player, boat);
        fireballSystem.enter(getApp());
    }

    void onImmediateGui() override
    {
        our::Entity *player = nullptr;
        for (auto entity : world.getEntities())
        {
            if (entity->name == "player")
            {
                player = entity;
                break;
            }
        }
        if (player)
        {
            if (auto inv = player->getComponent<our::InventoryComponent>())
            {
                auto size = getApp()->getFrameBufferSize();
                float tbY = size.y - 80.0f;

                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
                if (inv->woodCount > 0)
                {
                    ImGui::SetNextWindowPos(ImVec2(58, tbY - 51));
                    ImGui::SetNextWindowBgAlpha(0.0f);
                    ImGui::Begin("WoodCounter", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
                    ImGui::Text("%d", inv->woodCount);
                    ImGui::End();
                }

                if (inv->fishCount > 0)
                {
                    ImGui::SetNextWindowPos(ImVec2(148, tbY - 51));
                    ImGui::SetNextWindowBgAlpha(0.0f);
                    ImGui::Begin("FishCounter", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
                    ImGui::Text("%d", inv->fishCount);
                    ImGui::End();
                }
                ImGui::PopStyleColor();

                // Devil fruit pickup notification
                if (fireballSystem.shouldShowDevilFruitMessage())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 0, 255));
                    ImVec2 center = ImVec2(size.x * 0.5f, size.y * 0.3f);
                    ImGui::SetNextWindowPos(center, 0, ImVec2(0.5f, 0.5f));
                    ImGui::SetNextWindowBgAlpha(0.5f);
                    ImGui::Begin("DevilFruitMsg", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
                    ImGui::Text("You now possess devil fruit powers! Press 5 to try it!");
                    ImGui::End();
                    ImGui::PopStyleColor();
                }
            }
        }
    }

    void onDraw(double deltaTime) override
    {
        // Here, we just run a bunch of systems to control the world logic
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);
        marineBoatSystem.update(&world, (float)deltaTime);
        survivalSystem.update();
        animationSystem.update(&world, (float)deltaTime);
        combatSystem.update(&world, (float)deltaTime);
        fireballSystem.update(&world, (float)deltaTime);

        world.deleteMarkedEntities();

        // And finally we use the renderer system to draw the scene
        renderer.render(&world);

        // Get a reference to the keyboard object
        auto &keyboard = getApp()->getKeyboard();

        if (keyboard.justPressed(GLFW_KEY_ESCAPE))
        {
            // If the escape  key is pressed in this frame, go to the play state
            getApp()->changeState("menu");
        }
    }

    void onDestroy() override
    {
        // Don't forget to destroy the renderer
        renderer.destroy();
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Clear the world
        world.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};

// Force build 2
