#pragma once

#include <ecs/world.hpp>
#include <components/health.hpp>
#include <components/inventory.hpp>
#include <components/octopus-component.hpp>
#include <systems/boss-health-bar.hpp>
#include <systems/damage-flash.hpp>
#include <systems/fireball-system.hpp>
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace our {

    // Handles all ImGui overlays and screen-space rendering for the play state.
    // Keeps play-state.hpp free of drawing code.
    class GameUI {
    public:
        BossHealthBar bossHealthBar;
        DamageFlash damageFlash;

        void init(int screenWidth, int screenHeight) {
            bossHealthBar.init(screenWidth, screenHeight);
            damageFlash.init();
        }

        void resize(int screenWidth, int screenHeight) {
            bossHealthBar.resize(screenWidth, screenHeight);
        }

        void destroy() {
            bossHealthBar.destroy();
            damageFlash.destroy();
        }

        // ── Crosshair ──────────────────────────────────────────────────────
        void drawCrosshair(World &world, GLFWwindow *window, int screenW, int screenH) {
            ImVec2 center = ImVec2(screenW * 0.5f, screenH * 0.5f);
            ImU32 color = IM_COL32(255, 255, 255, 255);

            for (auto entity : world.getEntities()) {
                if (entity->name == "player") {
                    if (auto inv = entity->getComponent<InventoryComponent>()) {
                        // Mouse is checked via raw GLFW — we don't have Mouse ref here,
                        // so we check GLFW directly
                        if ((inv->activeSlot == 3 || inv->activeSlot == 5) &&
                            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS) {
                            color = IM_COL32(255, 0, 0, 255);
                        }
                    }
                    break;
                }
            }
            ImGui::GetForegroundDrawList()->AddCircleFilled(center, 3.0f, color);
        }

        // ── Inventory HUD (wood/fish counters, devil fruit message) ─────────
        void drawInventoryHUD(World &world, FireballSystem &fireballSystem, int screenW, int screenH) {
            for (auto entity : world.getEntities()) {
                if (entity->name != "player") continue;
                auto inv = entity->getComponent<InventoryComponent>();
                if (!inv) break;

                float tbY = screenH - 80.0f;

                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
                if (inv->woodCount > 0) {
                    ImGui::SetNextWindowPos(ImVec2(58, tbY - 51));
                    ImGui::SetNextWindowBgAlpha(0.0f);
                    ImGui::Begin("WoodCounter", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
                    ImGui::Text("%d", inv->woodCount);
                    ImGui::End();
                }
                if (inv->fishCount > 0) {
                    ImGui::SetNextWindowPos(ImVec2(148, tbY - 51));
                    ImGui::SetNextWindowBgAlpha(0.0f);
                    ImGui::Begin("FishCounter", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
                    ImGui::Text("%d", inv->fishCount);
                    ImGui::End();
                }
                ImGui::PopStyleColor();

                if (fireballSystem.shouldShowDevilFruitMessage()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 0, 255));
                    ImVec2 center = ImVec2(screenW * 0.5f, screenH * 0.3f);
                    ImGui::SetNextWindowPos(center, 0, ImVec2(0.5f, 0.5f));
                    ImGui::SetNextWindowBgAlpha(0.5f);
                    ImGui::Begin("DevilFruitMsg", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
                    ImGui::Text("You now possess devil fruit powers! Press 5 to try it!");
                    ImGui::End();
                    ImGui::PopStyleColor();
                }
                break;
            }
        }

        // ── Phase timer (marines only) ──────────────────────────────────────
        void drawMarineTimer(float remainingTime, int screenW) {
            int mins = (int)remainingTime / 60;
            int secs = (int)remainingTime % 60;
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 50, 50, 255));
            ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, 20.0f), 0, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.4f);
            ImGui::Begin("PhaseTimer", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("MARINE ASSAULT - %d:%02d", mins, secs);
            ImGui::End();
            ImGui::PopStyleColor();
        }

        // ── YOU LOSE ─────────────────────────────────────────────────────────
        void drawGameOver(const std::string &reason, int screenW, int screenH) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 30, 30, 255));
            ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.35f), 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.6f);
            ImGui::Begin("YouLose", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::SetWindowFontScale(3.0f);
            ImGui::Text("YOU LOSE");
            ImGui::SetWindowFontScale(1.5f);
            ImGui::Text("%s", reason.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Text("Press R to restart or ESC to quit");
            ImGui::End();
            ImGui::PopStyleColor();
        }

        // ── HEAL TO CONTINUE ────────────────────────────────────────────────
        void drawHealGate(int screenW, int screenH) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 50, 255));
            ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.25f), 0, ImVec2(0.5f, 0.5f));
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

        // ── COOLDOWN (initial phase) ────────────────────────────────────────
        void drawCooldown(int screenW, int screenH) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 200, 255, 255));
            ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.25f), 0, ImVec2(0.5f, 0.5f));
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

        // ── Damage flash (player hit) ───────────────────────────────────────
        void updateDamageFlash(World &world, float deltaTime, int screenW, int screenH) {
            for (auto entity : world.getEntities()) {
                if (entity->name == "player") {
                    auto hp = entity->getComponent<HealthComponent>();
                    if (hp && hp->justDamaged) {
                        damageFlash.triggerHit();
                        hp->justDamaged = false;
                    }
                    break;
                }
            }
            damageFlash.update(deltaTime);
            damageFlash.render(screenW, screenH);
        }

        // ── Boss health bar (octopus phase) ─────────────────────────────────
        void updateBossHealthBar(World &world, KrakenPhase &krakenPhase, float deltaTime, int screenW, int screenH) {
            resize(screenW, screenH);
            krakenPhase.onGui(&world, bossHealthBar, deltaTime);
        }
    };

} // namespace our
