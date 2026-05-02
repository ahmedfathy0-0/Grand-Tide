#pragma once

#include <application.hpp>
#include <ecs/world.hpp>
#include <systems/survival-system.hpp>
#include <systems/fireball-system.hpp>
#include <systems/ocean-audio.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/player-controller-system.hpp>
#include <systems/movement.hpp>
#include <systems/animation-system.hpp>
#include <systems/combat-system.hpp>
#include <systems/resource-spawner.hpp>
#include <loading-screen.hpp>
#include <phase-select-menu.hpp>
#include <pause-menu.hpp>
#include <asset-loader.hpp>
#include <components/health.hpp>
#include <components/octopus-component.hpp>
#include <input/keyboard.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

namespace our {

    // Manages game flow: start menu → loading → phase select → gameplay.
    // Handles pause, game-over, ESC, and reset logic.
    // Pure logic — no ImGui rendering (that's in GameUI).
    class GameFlowController {
    public:
        // Sub-systems owned here
        World world;
        ForwardRenderer renderer;
        PlayerControllerSystem cameraController;
        MovementSystem movementSystem;
        SurvivalSystem survivalSystem;
        AnimationSystem animationSystem;
        CombatSystem combatSystem;
        FireballSystem fireballSystem;
        ResourceSpawner resourceSpawner;
        OceanAudio oceanAudio;

        // UI overlays (input + render — but render is delegated to GameUI)
        LoadingScreen loadingScreen;
        PhaseSelectMenu phaseSelectMenu;
        PauseMenu pauseMenu;

        // Flow state
        bool isStartMenu = false;
        bool isLoading = false;
        bool loadingDone = false;
        int loadingStep = -1;
        float loadingProgress = 0.0f;
        float loadingTimer = 0.0f;
        bool isPhaseSelectMenu = false;

        bool gameOver = false;
        bool gameWon = false;
        std::string loseReason;

        // ESC tracking (bypasses ImGui keyboard capture)
        bool escPrev = false;
        bool escJustPressed(GLFWwindow *window) {
            bool cur = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            bool just = cur && !escPrev;
            escPrev = cur;
            return just;
        }

        // Asset loading guards
        static inline bool assetsLoaded = false;
        static inline bool rendererInitialized = false;

        void init() {
            isStartMenu = false;
            isLoading = false;
            loadingDone = false;
            loadingStep = -1;
            loadingProgress = 0.0f;
            loadingTimer = 0.0f;
            isPhaseSelectMenu = false;
            gameOver = false;
            gameWon = false;
            loseReason.clear();
            escPrev = false;
            fireballSystem.reset();
            cameraController.reset();

            loadingScreen.init();
            phaseSelectMenu.init();
            pauseMenu.init();
        }

        void destroy() {
            loadingScreen.destroy();
            phaseSelectMenu.destroy();
            pauseMenu.destroy();
            renderer.destroy();
            cameraController.exit();
            oceanAudio.stop();
            world.clear();
        }

        // ── Reset game without reloading assets ────────────────────────────
        void resetGame(Application *app) {
            world.clear();
            auto &config = app->getConfig()["scene"];
            if (config.contains("world"))
                world.deserialize(config["world"]);

            gameOver = false;
            gameWon = false;
            loseReason.clear();
            escPrev = false;

            cameraController.enter(app);
            fireballSystem.enter(app);

            Entity *player = nullptr, *boat = nullptr;
            for (auto entity : world.getEntities()) {
                if (entity->name == "player") player = entity;
                if (entity->name == "raft") boat = entity;
            }
            survivalSystem.setup(&world, app, player, boat);
        }

        // ── Lose condition check ──────────────────────────────────────────
        // Returns true if a lose condition was triggered this frame
        bool checkLoseCondition(World &world) {
            Entity *player = nullptr, *raft = nullptr;
            for (auto entity : world.getEntities()) {
                if (entity->name == "player") player = entity;
                if (entity->name == "raft") raft = entity;
            }
            if (player) {
                auto *ph = player->getComponent<HealthComponent>();
                if (ph && ph->isDead()) {
                    gameOver = true;
                    loseReason = "The pirate has fallen!";
                    std::cout << "[Lose] Player died!" << std::endl;
                    return true;
                }
            }
            if (raft) {
                auto *rh = raft->getComponent<HealthComponent>();
                if (rh && rh->isDead()) {
                    gameOver = true;
                    loseReason = "The raft has been destroyed!";
                    std::cout << "[Lose] Raft destroyed!" << std::endl;
                    return true;
                }
            }
            return false;
        }

        // ── DEBUG: F1 deals 50 damage to octopus ───────────────────────────
        void debugDamageOctopus(World &world, Keyboard &keyboard) {
            if (keyboard.justPressed(GLFW_KEY_F1)) {
                for (auto entity : world.getEntities()) {
                    if (entity->name == "octopus") {
                        auto hp = entity->getComponent<HealthComponent>();
                        auto oct = entity->getComponent<OctopusComponent>();
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
    };

} // namespace our
