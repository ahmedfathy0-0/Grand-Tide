#pragma once

#include <application.hpp>
#include "game-flow-controller.hpp"
#include "game-phase-manager.hpp"
#include <systems/game-ui.hpp>
#include <systems/elgembellias-system.hpp>
#include <imgui.h>

class Playstate : public our::State
{
    our::GameFlowController flow;
    our::GamePhaseManager phaseMgr;
    our::GameUI ui;
    our::ElgembelliasSystem elgembelliasSystem;

    void onInitialize() override
    {
        flow.init();
        phaseMgr.loadConfigs();
        phaseMgr.reset();

        // Skip internal start menu — Menustate handles that.
        // Go straight to loading.
        flow.isStartMenu = false;
        flow.isLoading = true;
        flow.loadingStep = -1;
        flow.loadingProgress = 0.0f;
        flow.loadingTimer = 0.0f;
        flow.loadingDone = false;
    }

    void onImmediateGui() override
    {
        auto size = getApp()->getFrameBufferSize();
        int screenW = size.x, screenH = size.y;
        auto &world = flow.world;

        // Don't draw game UI while loading or in phase-select
        if (flow.isLoading && !flow.loadingDone)
            return;
        if (flow.isPhaseSelectMenu) {
            flow.phaseSelectMenu.renderImGuiOverlay((float)screenW, (float)screenH);
            return;
        }

        // Crosshair
        ui.drawCrosshair(world, getApp()->getWindow(), screenW, screenH);

        // Inventory HUD
        ui.drawInventoryHUD(world, flow.fireballSystem, screenW, screenH);

        // Marine timer
        if (phaseMgr.currentPhase == our::GamePhaseManager::Phase::MARINES && !flow.pauseMenu.paused()) {
            ui.drawMarineTimer(phaseMgr.marinePhase.getRemainingTime(), screenW);
        }

        // Game over
        if (flow.gameOver) {
            ui.drawGameOver(flow.loseReason, screenW, screenH);
        }

        // Heal gate
        if (phaseMgr.waitingForHeal && !flow.gameOver) {
            ui.drawHealGate(screenW, screenH);
        }

        // Cooldown
        if (phaseMgr.currentPhase == our::GamePhaseManager::Phase::COOLDOWN && !flow.gameOver) {
            ui.drawCooldown(screenW, screenH);
        }
    }

    void onDraw(double deltaTime) override
    {
        auto &keyboard = getApp()->getKeyboard();
        auto &world = flow.world;
        float dt = (float)deltaTime;
        auto size = getApp()->getFrameBufferSize();

        // ═══════════════════════════════════════════════════════════════════
        //  LOADING SCREEN
        // ═══════════════════════════════════════════════════════════════════
        if (flow.isLoading && !flow.loadingDone) {
            auto &config = getApp()->getConfig()["scene"];
            flow.loadingTimer += dt;

            if (flow.loadingStep == -1) {
                flow.loadingProgress = 0.0f;
                flow.loadingTimer = 0.0f;
                flow.loadingStep = 0;
            }
            else if (flow.loadingStep == 0) {
                flow.loadingProgress = 0.1f;
                if (!flow.assetsLoaded) {
                    if (config.contains("assets"))
                        our::deserializeAllAssets(config["assets"]);
                    flow.assetsLoaded = true;
                }
                flow.loadingStep = 1;
                flow.loadingTimer = 0.0f;
            }
            else if (flow.loadingStep == 1) {
                flow.loadingProgress = 0.4f;
                world.clear();
                if (config.contains("world"))
                    world.deserialize(config["world"]);
                flow.cameraController.enter(getApp());
                flow.loadingStep = 2;
                flow.loadingTimer = 0.0f;
            }
            else if (flow.loadingStep == 2) {
                flow.loadingProgress = 0.7f;
                if (!flow.rendererInitialized) {
                    flow.renderer.initialize(size, config["renderer"]);
                    flow.rendererInitialized = true;
                }
                ui.init(size.x, size.y);
                our::Entity *player = nullptr, *boat = nullptr;
                for (auto entity : world.getEntities()) {
                    if (entity->name == "player") player = entity;
                    if (entity->name == "raft") boat = entity;
                }
                flow.survivalSystem.setup(&world, getApp(), player, boat);
                flow.fireballSystem.enter(getApp());
                flow.loadingStep = 3;
                flow.loadingTimer = 0.0f;
            }
            else if (flow.loadingStep == 3) {
                flow.loadingProgress = 0.85f + 0.15f * glm::min(flow.loadingTimer / 1.5f, 1.0f);
                if (flow.loadingTimer >= 1.5f) {
                    flow.loadingProgress = 1.0f;
                    flow.loadingDone = true;
                    flow.isLoading = false;
                    flow.isPhaseSelectMenu = true;
                }
            }

            flow.loadingScreen.render(flow.loadingProgress);
            return;
        }

        // ═══════════════════════════════════════════════════════════════════
        //  PHASE SELECTION MENU
        // ═══════════════════════════════════════════════════════════════════
        if (flow.isPhaseSelectMenu) {
            int action = flow.phaseSelectMenu.handleInput(keyboard);
            if (action == 1) { // Phase selected
                flow.isPhaseSelectMenu = false;
                int idx = flow.phaseSelectMenu.getSelectedIndex();
                flow.oceanAudio.start();
                phaseMgr.startPhase(idx, world, flow.fireballSystem);
            } else if (action == -1) { // ESC — go back to main menu
                flow.isPhaseSelectMenu = false;
                getApp()->changeState("menu");
                return;
            }
            flow.phaseSelectMenu.render();
            return;
        }

        // ═══════════════════════════════════════════════════════════════════
        //  GAME OVER / VICTORY
        // ═══════════════════════════════════════════════════════════════════
        if (flow.gameOver) {
            flow.renderer.render(&world);
            if (keyboard.justPressed(GLFW_KEY_R)) {
                flow.resetGame(getApp());
                phaseMgr.reset();
            }
            if (flow.escJustPressed(getApp()->getWindow())) {
                getApp()->close();
            }
            return;
        }

        // Victory: transition to end state
        if (flow.gameWon) {
            getApp()->changeState("end");
            return;
        }

        // ═══════════════════════════════════════════════════════════════════
        //  PAUSE MENU
        // ═══════════════════════════════════════════════════════════════════
        if (flow.escJustPressed(getApp()->getWindow())) {
            flow.pauseMenu.togglePause();
        }
        int menuAction = flow.pauseMenu.handleInput(keyboard);
        if (menuAction == 2) { flow.resetGame(getApp()); phaseMgr.reset(); return; }
        if (menuAction == 3) { getApp()->close(); return; }
        if (flow.pauseMenu.paused()) {
            flow.renderer.render(&world);
            flow.pauseMenu.renderOverlay();
            return;
        }

        // ═══════════════════════════════════════════════════════════════════
        //  LOSE CONDITION
        // ═══════════════════════════════════════════════════════════════════
        if (flow.checkLoseCondition(world))
            return;

        // ═══════════════════════════════════════════════════════════════════
        //  ENTITY VISIBILITY
        // ═══════════════════════════════════════════════════════════════════
        phaseMgr.updateEntityVisibility(world);

        // ═══════════════════════════════════════════════════════════════════
        //  SYSTEM UPDATES
        // ═══════════════════════════════════════════════════════════════════
        flow.movementSystem.update(&world, dt);
        flow.cameraController.update(&world, dt);
        flow.survivalSystem.update();
        flow.animationSystem.update(&world, dt);
        flow.combatSystem.update(&world, dt);
        flow.fireballSystem.update(&world, dt);

        // ═══════════════════════════════════════════════════════════════════
        //  PHASE MANAGEMENT
        // ═══════════════════════════════════════════════════════════════════
        bool healHold = phaseMgr.update(world, dt, flow.fireballSystem, elgembelliasSystem, getApp());

        // Win condition: Elgembellias killed
        if (!flow.gameWon && phaseMgr.currentPhase == our::GamePhaseManager::Phase::ELGEMBELLIAS &&
            phaseMgr.isElgembelliasDead(world)) {
            flow.gameWon = true;
            std::cout << "[Win] Elgembellias defeated! YOU WIN!" << std::endl;
        }

        // ═══════════════════════════════════════════════════════════════════
        //  RESOURCE SPAWNING & CLEANUP
        // ═══════════════════════════════════════════════════════════════════
        flow.resourceSpawner.update(&world, dt);
        world.deleteMarkedEntities();

        // ═══════════════════════════════════════════════════════════════════
        //  RENDER
        // ═══════════════════════════════════════════════════════════════════
        flow.renderer.render(&world);

        // Damage flash
        ui.updateDamageFlash(world, dt, size.x, size.y);

        // Boss health bar (octopus phase)
        if (phaseMgr.currentPhase == our::GamePhaseManager::Phase::OCTOPUS) {
            ui.updateBossHealthBar(world, phaseMgr.krakenPhase, dt, size.x, size.y);
        }

        // DEBUG
        flow.debugDamageOctopus(world, keyboard);
    }

    void onDestroy() override
    {
        ui.destroy();
        flow.destroy();
    }
};
