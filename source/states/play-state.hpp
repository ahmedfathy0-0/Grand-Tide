#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/player-controller-system.hpp>
#include <systems/movement.hpp>
#include <systems/survival-system.hpp>
#include <systems/animation-system.hpp>
#include <systems/octopus-system.hpp>
#include <systems/elgembellias-system.hpp>
#include <systems/combat-system.hpp>
#include <systems/boss-health-bar.hpp>
#include <systems/damage-flash.hpp>
#include <systems/fireball-system.hpp>
#include <systems/resource-spawner.hpp>
#include <systems/ocean-audio.hpp>
#include <components/health.hpp>
#include <components/octopus-component.hpp>
#include <components/elgembellias-component.hpp>
#include <components/musket-component.hpp>
#include <components/marine-boat-component.hpp>
#include <components/mesh-renderer.hpp>
#include <components/animator.hpp>
#include <components/enemy.hpp>
#include <mesh/model.hpp>
#include <asset-loader.hpp>
#include <start-menu.hpp>
#include <loading-screen.hpp>
#include <phase-select-menu.hpp>
#include <pause-menu.hpp>
#include <json-utils.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <iostream>
#include <unordered_map>

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
    our::FireballSystem fireballSystem;
    our::ResourceSpawner resourceSpawner;
    our::OceanAudio oceanAudio;
    our::ElgembelliasSystem elgembelliasSystem;

    // UI overlays (each self-contained)
    our::StartMenu startMenu;
    our::LoadingScreen loadingScreen;
    our::PhaseSelectMenu phaseSelectMenu;
    our::PauseMenu pauseMenu;
    our::BossHealthBar bossHealthBar;
    our::DamageFlash damageFlash;

    // ===== Game Phase Management =====
    enum class GamePhase { COOLDOWN, SHARKS, MARINES, OCTOPUS, ELGEMBELLIAS };
    GamePhase currentPhase = GamePhase::COOLDOWN;
    bool phaseTransitioning = false;

    our::SharkPhase sharkPhase;
    our::MarinePhase marinePhase;
    our::KrakenPhase krakenPhase;

    nlohmann::json sharksConfig;
    nlohmann::json marinesConfig;
    nlohmann::json krakenConfig;

    // ===== Game State =====
    bool gameOver = false;
    std::string loseReason;
    bool elgembelliasSpawned = false;

    // Healing gate: phase transition waits until player & raft are at 100% HP
    bool waitingForHeal = false;
    GamePhase nextPhaseAfterHeal = GamePhase::SHARKS;

    // ===== Menu State =====
    bool isStartMenu = true;
    bool isLoading = false;
    bool loadingDone = false;
    int loadingStep = -1; // -1=display only, 0=assets, 1=world, 2=renderer, 3=hold_end
    float loadingProgress = 0.0f;
    float loadingTimer = 0.0f;
    bool isPhaseSelectMenu = false;

    // Track whether assets have been loaded (skip re-loading on restart for speed)
    static inline bool assetsLoaded = false;
    static inline bool rendererInitialized = false;

    // Store original scales for entities we hide/show
    std::unordered_map<our::Entity*, glm::vec3> hiddenEntityScales;

    // Direct ESC tracking (bypasses ImGui keyboard capture)
    bool escPrev = false;
    bool escJustPressed() {
        bool cur = glfwGetKey(getApp()->getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
        bool just = cur && !escPrev;
        escPrev = cur;
        return just;
    }

    // ===== Entity show/hide helpers =====
    void hideEntity(our::Entity* e) {
        if (hiddenEntityScales.find(e) == hiddenEntityScales.end())
            hiddenEntityScales[e] = e->localTransform.scale;
        e->localTransform.scale = {0, 0, 0};
    }
    void showEntity(our::Entity* e) {
        auto it = hiddenEntityScales.find(e);
        if (it != hiddenEntityScales.end()) {
            e->localTransform.scale = it->second;
            hiddenEntityScales.erase(it);
        }
    }
    void hideEntityTree(our::Entity* e, our::World* w) {
        hideEntity(e);
        for (auto child : w->getEntities()) {
            if (child->parent == e) hideEntity(child);
        }
    }
    void showEntityTree(our::Entity* e, our::World* w) {
        showEntity(e);
        for (auto child : w->getEntities()) {
            if (child->parent == e) showEntity(child);
        }
    }

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
        // Reset all game state
        currentPhase = GamePhase::COOLDOWN;
        phaseTransitioning = false;
        gameOver = false;
        loseReason.clear();
        waitingForHeal = false;
        elgembelliasSpawned = false;
        escPrev = false;
        hiddenEntityScales.clear();
        fireballSystem.reset();
        cameraController.reset();

        // Menu state
        isStartMenu = true;
        isLoading = false;
        loadingDone = false;
        loadingStep = -1;
        loadingProgress = 0.0f;
        loadingTimer = 0.0f;
        isPhaseSelectMenu = false;

        // Heavy loading (assets, world, renderer) is deferred to the loading screen.
        // Only load lightweight configs and UI overlays here so the start menu appears instantly.

        // Load phase configs (lightweight JSON reads)
        sharksConfig = our::loadJsonFile("config/sharks-phase.json");
        marinesConfig = our::loadJsonFile("config/marines-phase.json");
        krakenConfig = our::loadJsonFile("config/kraken-phase.json");

        // Initialize self-contained UI overlays (each loads its own textures/shaders)
        // Note: oceanAudio.start() is deferred until gameplay actually begins (after phase selection)
        startMenu.init();
        loadingScreen.init();
        phaseSelectMenu.init();
        pauseMenu.init();
    }

    void onImmediateGui() override
    {
        // --- Loading Screen Overlay ---
        if (isLoading && !loadingDone) 
            return; // Don't draw game UI while loading
        // --- Crosshair ---
        {
            auto size = getApp()->getFrameBufferSize();
            ImVec2 center = ImVec2(size.x * 0.5f, size.y * 0.5f);
            ImU32 crosshairColor = IM_COL32(255, 255, 255, 255); // White by default

            our::Entity *player = nullptr;
            for (auto entity : world.getEntities()) {
                if (entity->name == "player") {
                    player = entity;
                    break;
                }
            }

            if (player) {
                if (auto inv = player->getComponent<our::InventoryComponent>()) {
                    auto& mouse = getApp()->getMouse();
                    if ((inv->activeSlot == 3 || inv->activeSlot == 5) && mouse.isPressed(GLFW_MOUSE_BUTTON_1)) {
                        crosshairColor = IM_COL32(255, 0, 0, 255); // Red when attacking
                    }
                }
            }
            ImGui::GetForegroundDrawList()->AddCircleFilled(center, 3.0f, crosshairColor);
        }

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

        // --- Phase Timer (MARINES phase only, hidden when paused) ---
        if (currentPhase == GamePhase::MARINES && !pauseMenu.paused()) {
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

        // --- Phase Selection Menu ImGui overlay ---
        if (isPhaseSelectMenu) {
            auto size = getApp()->getFrameBufferSize();
            phaseSelectMenu.renderImGuiOverlay((float)size.x, (float)size.y);
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

        // --- COOLDOWN overlay (initial phase before sharks, only when starting from sharks) ---
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
        elgembelliasSpawned = false;
        escPrev = false;
        hiddenEntityScales.clear();

        cameraController.enter(getApp());
        fireballSystem.enter(getApp());

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

        // ===== START MENU =====
        if (isStartMenu) {
            int action = startMenu.handleInput(keyboard);
            if (action == 1) { // Play
                isStartMenu = false;
                isLoading = true;
                loadingStep = -1;
                loadingProgress = 0.0f;
                loadingTimer = 0.0f;
                loadingDone = false;
            } else if (action == 2) { // Exit
                getApp()->close();
                return;
            }
            startMenu.render();
            return;
        }

        // ===== LOADING SCREEN =====
        if (isLoading && !loadingDone) {
            auto &config = getApp()->getConfig()["scene"];
            loadingTimer += (float)deltaTime;

            if (loadingStep == -1) {
                loadingProgress = 0.0f;
                loadingTimer = 0.0f;
                loadingStep = 0;
            }
            else if (loadingStep == 0) {
                loadingProgress = 0.1f;
                if (!assetsLoaded) {
                    if (config.contains("assets"))
                        our::deserializeAllAssets(config["assets"]);
                    assetsLoaded = true;
                }
                loadingStep = 1;
                loadingTimer = 0.0f;
            }
            else if (loadingStep == 1) {
                loadingProgress = 0.4f;
                world.clear();
                if (config.contains("world"))
                    world.deserialize(config["world"]);
                cameraController.enter(getApp());
                loadingStep = 2;
                loadingTimer = 0.0f;
            }
            else if (loadingStep == 2) {
                loadingProgress = 0.7f;
                auto size = getApp()->getFrameBufferSize();
                if (!rendererInitialized) {
                    renderer.initialize(size, config["renderer"]);
                    rendererInitialized = true;
                }
                bossHealthBar.init(size.x, size.y);
                damageFlash.init();
                our::Entity *player = nullptr, *boat = nullptr;
                for (auto entity : world.getEntities()) {
                    if (entity->name == "player") player = entity;
                    if (entity->name == "raft") boat = entity;
                }
                survivalSystem.setup(&world, getApp(), player, boat);
                fireballSystem.enter(getApp());
                loadingStep = 3;
                loadingTimer = 0.0f;
            }
            else if (loadingStep == 3) {
                loadingProgress = 0.85f + 0.15f * glm::min(loadingTimer / 1.5f, 1.0f);
                if (loadingTimer >= 1.5f) {
                    loadingProgress = 1.0f;
                    loadingDone = true;
                    isLoading = false;
                    isPhaseSelectMenu = true;
                }
            }

            loadingScreen.render(loadingProgress);
            return;
        }

        // ===== PHASE SELECTION MENU =====
        if (isPhaseSelectMenu) {
            int action = phaseSelectMenu.handleInput(keyboard);
            if (action == 1) { // Phase selected
                isPhaseSelectMenu = false;
                int idx = phaseSelectMenu.getSelectedIndex();
                // Start ocean audio only when gameplay begins
                oceanAudio.start();
                switch (idx) {
                    case 0:
                        currentPhase = GamePhase::COOLDOWN; // Sharks goes through cooldown first
                        break;
                    case 1:
                        currentPhase = GamePhase::MARINES;
                        marinePhase.enter(&world, marinesConfig);
                        fireballSystem.grantDevilFruit(&world);
                        break;
                    case 2:
                        currentPhase = GamePhase::OCTOPUS;
                        krakenPhase.enter(&world, krakenConfig);
                        fireballSystem.grantDevilFruit(&world);
                        break;
                }
                phaseTransitioning = false;
                std::cout << "[Phase] Starting from phase: " << idx << std::endl;
            } else if (action == -1) { // ESC — go back
                isPhaseSelectMenu = false;
                isStartMenu = true;
            }
            phaseSelectMenu.render();
            return;
        }

        // ===== GAME OVER =====
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

        // ===== PAUSE MENU =====
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

        // ===== LOSE CONDITION CHECK =====
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

        // ===== ENTITY VISIBILITY (phase-aware) =====
        // COOLDOWN/SHARKS: hide marines, octopus, elgembellias
        if (currentPhase == GamePhase::COOLDOWN || currentPhase == GamePhase::SHARKS) {
            for (auto entity : world.getEntities()) {
                if (entity->getComponent<our::OctopusComponent>()) hideEntity(entity);
                if (entity->getComponent<our::MarineBoatComponent>()) hideEntityTree(entity, &world);
                if (entity->name == "debug_marine") hideEntity(entity);
                if (entity->getComponent<our::MusketComponent>() && !entity->parent) hideEntity(entity);
                if (entity->getComponent<our::ElgembelliasComponent>()) hideEntity(entity);
            }
        }
        else if (currentPhase == GamePhase::MARINES) {
            for (auto entity : world.getEntities()) {
                if (entity->getComponent<our::OctopusComponent>()) hideEntity(entity);
                if (entity->getComponent<our::MarineBoatComponent>()) showEntityTree(entity, &world);
                if (entity->name == "debug_marine") showEntity(entity);
                if (entity->getComponent<our::MusketComponent>() && !entity->parent) showEntity(entity);
                if (entity->getComponent<our::ElgembelliasComponent>()) hideEntity(entity);
            }
        } else if (currentPhase == GamePhase::OCTOPUS) {
            for (auto entity : world.getEntities()) {
                if (entity->getComponent<our::OctopusComponent>()) showEntity(entity);
                if (entity->getComponent<our::MarineBoatComponent>()) hideEntityTree(entity, &world);
                if (entity->name == "debug_marine") hideEntity(entity);
                if (entity->getComponent<our::MusketComponent>() && !entity->parent) hideEntity(entity);
                if (entity->getComponent<our::ElgembelliasComponent>()) hideEntity(entity);
            }
        } else if (currentPhase == GamePhase::ELGEMBELLIAS) {
            for (auto entity : world.getEntities()) {
                if (entity->getComponent<our::ElgembelliasComponent>()) showEntity(entity);
            }
        }

        // ===== SYSTEM UPDATES =====
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);
        survivalSystem.update();
        animationSystem.update(&world, (float)deltaTime);
        combatSystem.update(&world, (float)deltaTime);
        fireballSystem.update(&world, (float)deltaTime);

        // ===== HEALING GATE =====
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

        // ===== PHASE MANAGEMENT =====
        if (currentPhase == GamePhase::COOLDOWN) {
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
                waitingForHeal = true;
                nextPhaseAfterHeal = GamePhase::MARINES;
                phaseTransitioning = false;
                std::cout << "[Phase] === DEVIL FRUIT SPAWNED === Heal to enter MARINES!" << std::endl;
            }
        }
        else if (currentPhase == GamePhase::MARINES) {
            bool marinesDone = marinePhase.update(&world, (float)deltaTime);
            if (marinesDone && !phaseTransitioning) {
                phaseTransitioning = true;
                marinePhase.exit();
                waitingForHeal = true;
                nextPhaseAfterHeal = GamePhase::OCTOPUS;
                phaseTransitioning = false;
                std::cout << "[Phase] === MARINES PHASE OVER === Heal to enter OCTOPUS!" << std::endl;
            }
        }
        else if (currentPhase == GamePhase::OCTOPUS) {
            bool krakenDone = krakenPhase.update(&world, (float)deltaTime, getApp());
            // Check if octopus is permanently dead -> transition to ELGEMBELLIAS
            for (auto entity : world.getEntities()) {
                auto octComp = entity->getComponent<our::OctopusComponent>();
                if (octComp && octComp->permanentlyDead && !phaseTransitioning) {
                    phaseTransitioning = true;
                    krakenPhase.exit();
                    currentPhase = GamePhase::ELGEMBELLIAS;
                    elgembelliasSpawned = false;
                    phaseTransitioning = false;
                    std::cout << "[Phase] === OCTOPUS PERMANENTLY DEAD === Transitioning to ELGEMBELLIAS phase!" << std::endl;
                    break;
                }
            }
        }
        else if (currentPhase == GamePhase::ELGEMBELLIAS) {
            elgembelliasSystem.update(&world, (float)deltaTime, player);
            // Activate the Elgembellias entity (already deserialized from app.jsonc)
            if (!elgembelliasSpawned && raft) {
                glm::vec3 octopusDeathPos = raft->localTransform.position;
                for (auto entity : world.getEntities()) {
                    if (entity->getComponent<our::OctopusComponent>()) {
                        octopusDeathPos = entity->localTransform.position;
                        break;
                    }
                }
                elgembelliasSystem.activate(&world, octopusDeathPos);
                elgembelliasSpawned = true;
            }
        }

        // ===== RESOURCE SPAWNING & LIFETIME =====
        resourceSpawner.update(&world, (float)deltaTime);
        world.deleteMarkedEntities();

        // ===== RENDER =====
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
        startMenu.destroy();
        loadingScreen.destroy();
        phaseSelectMenu.destroy();
        pauseMenu.destroy();
        bossHealthBar.destroy();
        damageFlash.destroy();
        renderer.destroy();
        cameraController.exit();
        oceanAudio.stop();
        world.clear();
    }
};

