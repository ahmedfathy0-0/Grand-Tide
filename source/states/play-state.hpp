#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/player-controller-system.hpp>
#include <systems/movement.hpp>
#include <systems/survival-system.hpp>
#include <systems/animation-system.hpp>
#include <systems/combat-system.hpp>
#include <systems/boss-health-bar.hpp>
#include <systems/damage-flash.hpp>
#include <systems/fireball-system.hpp>
#include <systems/resource-spawner.hpp>
#include <systems/ocean-audio.hpp>
#include <components/health.hpp>
#include <components/octopus-component.hpp>
#include <asset-loader.hpp>
#include <gl-helpers.hpp>
#include <pause-menu.hpp>
#include <json-utils.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

// Phase handlers
#include "shark-state.hpp"
#include "marine-state.hpp"
#include "kraken-state.hpp"

class Playstate : public our::State
{
    our::World world;
    our::ForwardRenderer renderer;
    our::PlayerControllerSystem cameraController;
    our::MovementSystem movementSystem;
    our::SurvivalSystem survivalSystem;
    our::AnimationSystem animationSystem;
    our::CombatSystem combatSystem;
    our::BossHealthBar bossHealthBar;
    our::DamageFlash damageFlash;
    our::FireballSystem fireballSystem;
    our::ResourceSpawner resourceSpawner;
    our::OceanAudio oceanAudio;
    our::PauseMenu pauseMenu;

    // Game phases — delegated to phase handlers
    enum class GamePhase { COOLDOWN, SHARKS, MARINES, OCTOPUS };
    GamePhase currentPhase = GamePhase::COOLDOWN;
    bool phaseTransitioning = false;

    // Lose mechanic
    bool gameOver = false;
    std::string loseReason;

    // Direct ESC tracking (bypasses ImGui keyboard capture)
    bool escPrev = false;
    bool escJustPressed() {
        bool cur = glfwGetKey(getApp()->getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
        bool just = cur && !escPrev;
        escPrev = cur;
        return just;
    }

    // Healing gate: phase transition waits until player & raft are at 100% HP
    bool waitingForHeal = false;
    GamePhase nextPhaseAfterHeal = GamePhase::SHARKS; // which phase to enter after healed

    our::SharkPhase sharkPhase;
    our::MarinePhase marinePhase;
    our::KrakenPhase krakenPhase;

    nlohmann::json sharksConfig;
    nlohmann::json marinesConfig;
    nlohmann::json krakenConfig;

    // Check if both player and raft are at full health
    bool isFullyHealed() {
        our::Entity *player = nullptr, *raft = nullptr;
        for (auto entity : world.getEntities()) {
            if (entity->name == "player") player = entity;
            if (entity->name == "raft") raft = entity;
        }
        if (!player || !raft) return false;
        auto *ph = player->getComponent<our::HealthComponent>();
        auto *rh = raft->getComponent<our::HealthComponent>();
        if (!ph || !rh) return false;
        return ph->currentHealth >= ph->maxHealth && rh->currentHealth >= rh->maxHealth;
    }

    void onInitialize() override
    {
        auto &config = getApp()->getConfig()["scene"];
        if (config.contains("assets"))
            our::deserializeAllAssets(config["assets"]);
        if (config.contains("world"))
            world.deserialize(config["world"]);

        // Load phase configs
        sharksConfig = our::loadJsonFile("config/sharks-phase.json");
        marinesConfig = our::loadJsonFile("config/marines-phase.json");
        krakenConfig = our::loadJsonFile("config/kraken-phase.json");

        // Enter first phase (COOLDOWN - wait for full health before sharks)
        currentPhase = GamePhase::COOLDOWN;
        phaseTransitioning = false;
        gameOver = false;
        loseReason.clear();
        waitingForHeal = false;
        escPrev = false;

        cameraController.enter(getApp());
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
        bossHealthBar.init(size.x, size.y);
        damageFlash.init();

        our::Entity *player = nullptr, *boat = nullptr;
        for (auto entity : world.getEntities()) {
            if (entity->name == "player") player = entity;
            if (entity->name == "raft") boat = entity;
        }
        survivalSystem.setup(&world, getApp(), player, boat);

        oceanAudio.start();
        pauseMenu.init();
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

        // --- Phase Timer (MARINES phase only) ---
        if (currentPhase == GamePhase::MARINES) {
            auto size = getApp()->getFrameBufferSize();
            float remaining = marinePhase.getRemainingTime();
            int mins = (int)remaining / 60;
            int secs = (int)remaining % 60;

            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 50, 50, 255));
            ImGui::SetNextWindowPos(ImVec2(size.x * 0.5f, 20.0f), 0, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.4f);
            ImGui::Begin("PhaseTimer", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("MARINE ASSAULT - %d:%02d", mins, secs);
            ImGui::End();
            ImGui::PopStyleColor();
        }

        // --- YOU LOSE overlay ---
        if (gameOver) {
            auto size = getApp()->getFrameBufferSize();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 30, 30, 255));
            ImGui::SetNextWindowPos(ImVec2(size.x * 0.5f, size.y * 0.35f), 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.6f);
            ImGui::Begin("YouLose", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::SetWindowFontScale(3.0f);
            ImGui::Text("YOU LOSE");
            ImGui::SetWindowFontScale(1.5f);
            ImGui::Text("%s", loseReason.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Text("Press R to restart or ESC to quit");
            ImGui::End();
            ImGui::PopStyleColor();
        }

        // --- HEAL TO CONTINUE overlay ---
        if (waitingForHeal && !gameOver) {
            auto size = getApp()->getFrameBufferSize();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 50, 255));
            ImGui::SetNextWindowPos(ImVec2(size.x * 0.5f, size.y * 0.25f), 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.5f);
            ImGui::Begin("HealGate", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::SetWindowFontScale(1.8f);
            ImGui::Text("HEAL TO CONTINUE!");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Text("Player and Raft must be at full health");
            ImGui::Text("Eat fish (R) and repair raft (1 + Left Click)");
            ImGui::End();
            ImGui::PopStyleColor();
        }

        // --- COOLDOWN overlay (initial phase before sharks) ---
        if (currentPhase == GamePhase::COOLDOWN && !gameOver) {
            auto size = getApp()->getFrameBufferSize();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 200, 255, 255));
            ImGui::SetNextWindowPos(ImVec2(size.x * 0.5f, size.y * 0.25f), 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.5f);
            ImGui::Begin("CooldownGate", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::SetWindowFontScale(1.8f);
            ImGui::Text("PREPARE YOURSELF!");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Text("Heal up before the sharks arrive");
            ImGui::Text("Player and Raft must be at full health");
            ImGui::End();
            ImGui::PopStyleColor();
        }

    }

    void resetGame() {
        // Reset game state without reloading assets
        world.clear();
        auto &config = getApp()->getConfig()["scene"];
        if (config.contains("world"))
            world.deserialize(config["world"]);

        currentPhase = GamePhase::COOLDOWN;
        phaseTransitioning = false;
        gameOver = false;
        loseReason.clear();
        waitingForHeal = false;
        escPrev = false;

        cameraController.enter(getApp());
        survivalSystem.setup(&world, getApp(), nullptr, nullptr);
        fireballSystem.enter(getApp());

        // Re-find player and boat
        our::Entity *player = nullptr, *boat = nullptr;
        for (auto entity : world.getEntities()) {
            if (entity->name == "player") player = entity;
            if (entity->name == "raft") boat = entity;
        }
        survivalSystem.setup(&world, getApp(), player, boat);
    }

    void onDraw(double deltaTime) override
    {
        auto &keyboard = getApp()->getKeyboard();

        // --- Restart when game over ---
        if (gameOver) {
            renderer.render(&world);
            if (keyboard.justPressed(GLFW_KEY_R)) {
                resetGame();
            }
            if (escJustPressed()) {
                getApp()->close();
            }
            return;
        }

        // --- Pause Menu (use direct ESC check for robustness) ---
        if (escJustPressed()) {
            pauseMenu.togglePause();
        }
        int menuAction = pauseMenu.handleInput(keyboard);
        if (menuAction == 2) { resetGame(); return; }
        if (menuAction == 3) { getApp()->close(); return; }
        if (pauseMenu.paused()) {
            renderer.render(&world);
            pauseMenu.renderOverlay();
            return;
        }

        // --- Lose condition check ---
        our::Entity *player = nullptr, *raft = nullptr;
        for (auto entity : world.getEntities()) {
            if (entity->name == "player") player = entity;
            if (entity->name == "raft") raft = entity;
        }
        if (player) {
            auto *ph = player->getComponent<our::HealthComponent>();
            if (ph && ph->isDead()) {
                gameOver = true;
                loseReason = "The pirate has fallen!";
                std::cout << "[Lose] Player died!" << std::endl;
                return;
            }
        }
        if (raft) {
            auto *rh = raft->getComponent<our::HealthComponent>();
            if (rh && rh->isDead()) {
                gameOver = true;
                loseReason = "The raft has been destroyed!";
                std::cout << "[Lose] Raft destroyed!" << std::endl;
                return;
            }
        }

        // --- Normal game update ---
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);
        survivalSystem.update();
        animationSystem.update(&world, (float)deltaTime);
        combatSystem.update(&world, (float)deltaTime);
        fireballSystem.update(&world, (float)deltaTime);

        // === HEALING GATE: wait for full health before entering next phase ===
        if (waitingForHeal) {
            if (isFullyHealed()) {
                waitingForHeal = false;
                currentPhase = nextPhaseAfterHeal;
                if (currentPhase == GamePhase::MARINES) {
                    marinePhase.enter(&world, marinesConfig);
                    std::cout << "[Phase] === HEALED === Entering MARINES phase!" << std::endl;
                } else if (currentPhase == GamePhase::OCTOPUS) {
                    krakenPhase.enter(&world, krakenConfig);
                    std::cout << "[Phase] === HEALED === Entering OCTOPUS phase!" << std::endl;
                }
            }
            // Still update resources and render while waiting
            resourceSpawner.update(&world, (float)deltaTime);
            world.deleteMarkedEntities();
            renderer.render(&world);
            return;
        }

        // === PHASE MANAGEMENT ===
        if (currentPhase == GamePhase::COOLDOWN) {
            // Wait for player and raft to be fully healed before starting sharks
            if (isFullyHealed()) {
                currentPhase = GamePhase::SHARKS;
                sharkPhase.enter(&world, sharksConfig);
                std::cout << "[Phase] === HEALED === Entering SHARKS phase!" << std::endl;
            }
        }
        else if (currentPhase == GamePhase::SHARKS) {
            bool sharksDone = sharkPhase.update(&world, (float)deltaTime);
            bool fruitSpawned = fireballSystem.isDevilFruitSpawned();
            if (sharksDone && fruitSpawned && !phaseTransitioning) {
                phaseTransitioning = true;
                sharkPhase.exit();
                // Devil fruit spawned — wait for heal before marines
                waitingForHeal = true;
                nextPhaseAfterHeal = GamePhase::MARINES;
                phaseTransitioning = false;
                std::cout << "[Phase] === DEVIL FRUIT SPAWNED === Heal to enter MARINES!" << std::endl;
            } else if (sharksDone && !fruitSpawned) {
                // Sharks done but fruit not spawned yet — keep waiting
            }
        }
        else if (currentPhase == GamePhase::MARINES) {
            bool marinesDone = marinePhase.update(&world, (float)deltaTime);
            if (marinesDone && !phaseTransitioning) {
                phaseTransitioning = true;
                marinePhase.exit();
                // Wait for heal before kraken
                waitingForHeal = true;
                nextPhaseAfterHeal = GamePhase::OCTOPUS;
                phaseTransitioning = false;
                std::cout << "[Phase] === MARINES PHASE OVER === Heal to enter OCTOPUS!" << std::endl;
            }
        }
        else if (currentPhase == GamePhase::OCTOPUS) {
            bool krakenDone = krakenPhase.update(&world, (float)deltaTime, getApp());
            if (krakenDone && !phaseTransitioning) {
                phaseTransitioning = true;
                krakenPhase.exit();
            }
        }

        // === RESOURCE SPAWNING & LIFETIME ===
        resourceSpawner.update(&world, (float)deltaTime);
        world.deleteMarkedEntities();

        // === RENDER ===
        renderer.render(&world);

        // Damage flash overlay
        for (auto entity : world.getEntities()) {
            if (entity->name == "player") {
                auto hp = entity->getComponent<our::HealthComponent>();
                if (hp && hp->justDamaged) {
                    damageFlash.triggerHit();
                    hp->justDamaged = false;
                }
                break;
            }
        }
        damageFlash.update((float)deltaTime);
        {
            auto size = getApp()->getFrameBufferSize();
            damageFlash.render(size.x, size.y);
        }

        // Boss health bar (OCTOPUS phase only)
        if (currentPhase == GamePhase::OCTOPUS) {
            auto size = getApp()->getFrameBufferSize();
            bossHealthBar.resize(size.x, size.y);
            krakenPhase.onGui(&world, bossHealthBar, (float)deltaTime);
        }

        // DEBUG: F1 deals 50 damage to octopus
        if (keyboard.justPressed(GLFW_KEY_F1)) {
            for (auto entity : world.getEntities()) {
                if (entity->name == "octopus") {
                    auto hp = entity->getComponent<our::HealthComponent>();
                    auto oct = entity->getComponent<our::OctopusComponent>();
                    if (hp) {
                        float rawDamage = 50.0f;
                        float multiplier = (oct && oct->hasRevived) ? oct->damageMultiplier : 1.0f;
                        float finalDamage = rawDamage * multiplier;
                        hp->currentHealth -= finalDamage;
                        if (hp->currentHealth < 0) hp->currentHealth = 0;
                        std::cout << "[DEBUG] Dealt " << finalDamage << " damage to octopus (multiplier="
                                  << multiplier << "). HP: "
                                  << hp->currentHealth << " / " << hp->maxHealth << std::endl;
                    }
                    break;
                }
            }
        }
    }

    void onDestroy() override
    {
        renderer.destroy();
        bossHealthBar.destroy();
        damageFlash.destroy();
        pauseMenu.destroy();
        cameraController.exit();
        oceanAudio.stop();
        world.clear();
        our::clearAllAssets();
    }
};

