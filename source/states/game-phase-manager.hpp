#pragma once

#include <application.hpp>
#include <ecs/world.hpp>
#include <components/health.hpp>
#include <components/octopus-component.hpp>
#include <components/elgembellias-component.hpp>
#include <components/musket-component.hpp>
#include <components/marine-boat-component.hpp>
#include <systems/elgembellias-system.hpp>
#include <systems/fireball-system.hpp>
#include <json-utils.hpp>
#include <iostream>
#include <unordered_map>

// Phase handlers
#include "shark-state.hpp"
#include "marine-state.hpp"
#include "kraken-state.hpp"

namespace our {

    // Manages game phases, entity visibility, and healing gate logic.
    // Pure logic — no rendering.
    class GamePhaseManager {
    public:
        enum class Phase { COOLDOWN, SHARKS, MARINES, OCTOPUS, ELGEMBELLIAS };

        Phase currentPhase = Phase::COOLDOWN;
        bool phaseTransitioning = false;
        bool waitingForHeal = false;
        Phase nextPhaseAfterHeal = Phase::SHARKS;
        bool elgembelliasSpawned = false;

        SharkPhase sharkPhase;
        MarinePhase marinePhase;
        KrakenPhase krakenPhase;

        nlohmann::json sharksConfig;
        nlohmann::json marinesConfig;
        nlohmann::json krakenConfig;

        // Store original scales for entities we hide/show
        std::unordered_map<Entity*, glm::vec3> hiddenEntityScales;

        void loadConfigs() {
            sharksConfig = loadJsonFile("config/sharks-phase.json");
            marinesConfig = loadJsonFile("config/marines-phase.json");
            krakenConfig = loadJsonFile("config/kraken-phase.json");
        }

        void reset() {
            currentPhase = Phase::COOLDOWN;
            phaseTransitioning = false;
            waitingForHeal = false;
            elgembelliasSpawned = false;
            hiddenEntityScales.clear();
        }

        // ── Entity show/hide helpers ────────────────────────────────────────
        void hideEntity(Entity* e) {
            if (hiddenEntityScales.find(e) == hiddenEntityScales.end())
                hiddenEntityScales[e] = e->localTransform.scale;
            e->localTransform.scale = {0, 0, 0};
        }
        void showEntity(Entity* e) {
            auto it = hiddenEntityScales.find(e);
            if (it != hiddenEntityScales.end()) {
                e->localTransform.scale = it->second;
                hiddenEntityScales.erase(it);
            }
        }
        void hideEntityTree(Entity* e, World* w) {
            hideEntity(e);
            for (auto child : w->getEntities()) {
                if (child->parent == e) hideEntity(child);
            }
        }
        void showEntityTree(Entity* e, World* w) {
            showEntity(e);
            for (auto child : w->getEntities()) {
                if (child->parent == e) showEntity(child);
            }
        }

        // ── Check if both player and raft are at full health ────────────────
        bool isFullyHealed(World &world) {
            Entity *player = nullptr, *raft = nullptr;
            for (auto entity : world.getEntities()) {
                if (entity->name == "player") player = entity;
                if (entity->name == "raft") raft = entity;
            }
            if (!player || !raft) return false;
            auto *ph = player->getComponent<HealthComponent>();
            auto *rh = raft->getComponent<HealthComponent>();
            if (!ph || !rh) return false;
            return ph->currentHealth >= ph->maxHealth && rh->currentHealth >= rh->maxHealth;
        }

        // ── Entity visibility based on current phase ───────────────────────
        void updateEntityVisibility(World &world) {
            if (currentPhase == Phase::COOLDOWN || currentPhase == Phase::SHARKS) {
                for (auto entity : world.getEntities()) {
                    if (entity->getComponent<OctopusComponent>()) hideEntity(entity);
                    if (entity->getComponent<MarineBoatComponent>()) hideEntityTree(entity, &world);
                    if (entity->name == "debug_marine") hideEntity(entity);
                    if (entity->getComponent<MusketComponent>() && !entity->parent) hideEntity(entity);
                    if (entity->getComponent<ElgembelliasComponent>()) hideEntity(entity);
                }
            }
            else if (currentPhase == Phase::MARINES) {
                for (auto entity : world.getEntities()) {
                    if (entity->getComponent<OctopusComponent>()) hideEntity(entity);
                    if (entity->getComponent<MarineBoatComponent>()) showEntityTree(entity, &world);
                    if (entity->name == "debug_marine") showEntity(entity);
                    if (entity->getComponent<MusketComponent>() && !entity->parent) showEntity(entity);
                    if (entity->getComponent<ElgembelliasComponent>()) hideEntity(entity);
                }
            }
            else if (currentPhase == Phase::OCTOPUS) {
                for (auto entity : world.getEntities()) {
                    if (entity->getComponent<OctopusComponent>()) showEntity(entity);
                    if (entity->getComponent<MarineBoatComponent>()) hideEntityTree(entity, &world);
                    if (entity->name == "debug_marine") hideEntity(entity);
                    if (entity->getComponent<MusketComponent>() && !entity->parent) hideEntity(entity);
                    if (entity->getComponent<ElgembelliasComponent>()) hideEntity(entity);
                }
            }
            else if (currentPhase == Phase::ELGEMBELLIAS) {
                for (auto entity : world.getEntities()) {
                    if (entity->getComponent<ElgembelliasComponent>()) showEntity(entity);
                }
            }
        }

        // ── Phase management update ─────────────────────────────────────────
        // Returns true if the game should skip the rest of the frame (healing gate hold)
        bool update(World &world, float deltaTime, FireballSystem &fireballSystem, ElgembelliasSystem &elgembelliasSystem, Application *app) {
            // Healing gate: wait until fully healed before entering next phase
            if (waitingForHeal) {
                if (isFullyHealed(world)) {
                    waitingForHeal = false;
                    currentPhase = nextPhaseAfterHeal;
                    if (currentPhase == Phase::MARINES) {
                        marinePhase.enter(&world, marinesConfig);
                        std::cout << "[Phase] === HEALED === Entering MARINES phase!" << std::endl;
                    } else if (currentPhase == Phase::OCTOPUS) {
                        krakenPhase.enter(&world, krakenConfig);
                        std::cout << "[Phase] === HEALED === Entering OCTOPUS phase!" << std::endl;
                    }
                }
                return true; // hold frame
            }

            // Phase transitions
            if (currentPhase == Phase::COOLDOWN) {
                if (isFullyHealed(world)) {
                    currentPhase = Phase::SHARKS;
                    sharkPhase.enter(&world, sharksConfig);
                    std::cout << "[Phase] === HEALED === Entering SHARKS phase!" << std::endl;
                }
            }
            else if (currentPhase == Phase::SHARKS) {
                bool sharksDone = sharkPhase.update(&world, deltaTime);
                bool fruitSpawned = fireballSystem.isDevilFruitSpawned();
                if (sharksDone && fruitSpawned && !phaseTransitioning) {
                    phaseTransitioning = true;
                    sharkPhase.exit();
                    waitingForHeal = true;
                    nextPhaseAfterHeal = Phase::MARINES;
                    phaseTransitioning = false;
                    std::cout << "[Phase] === DEVIL FRUIT SPAWNED === Heal to enter MARINES!" << std::endl;
                }
            }
            else if (currentPhase == Phase::MARINES) {
                bool marinesDone = marinePhase.update(&world, deltaTime);
                if (marinesDone && !phaseTransitioning) {
                    phaseTransitioning = true;
                    marinePhase.exit();
                    waitingForHeal = true;
                    nextPhaseAfterHeal = Phase::OCTOPUS;
                    phaseTransitioning = false;
                    std::cout << "[Phase] === MARINES PHASE OVER === Heal to enter OCTOPUS!" << std::endl;
                }
            }
            else if (currentPhase == Phase::OCTOPUS) {
                bool krakenDone = krakenPhase.update(&world, deltaTime, app);
                for (auto entity : world.getEntities()) {
                    auto octComp = entity->getComponent<OctopusComponent>();
                    if (octComp && octComp->permanentlyDead && !phaseTransitioning) {
                        phaseTransitioning = true;
                        krakenPhase.exit();
                        currentPhase = Phase::ELGEMBELLIAS;
                        elgembelliasSpawned = false;
                        phaseTransitioning = false;
                        std::cout << "[Phase] === OCTOPUS PERMANENTLY DEAD === Transitioning to ELGEMBELLIAS phase!" << std::endl;
                        break;
                    }
                }
            }
            else if (currentPhase == Phase::ELGEMBELLIAS) {
                Entity *player = nullptr, *raft = nullptr;
                for (auto entity : world.getEntities()) {
                    if (entity->name == "player") player = entity;
                    if (entity->name == "raft") raft = entity;
                }
                elgembelliasSystem.update(&world, deltaTime, player);
                if (!elgembelliasSpawned && raft) {
                    glm::vec3 pos = raft->localTransform.position;
                    for (auto entity : world.getEntities()) {
                        if (entity->getComponent<OctopusComponent>()) {
                            pos = entity->localTransform.position;
                            break;
                        }
                    }
                    elgembelliasSystem.activate(&world, pos);
                    elgembelliasSpawned = true;
                }
                // Check if Elgembellias is dead -> victory
                for (auto entity : world.getEntities()) {
                    auto elgComp = entity->getComponent<ElgembelliasComponent>();
                    if (elgComp && elgComp->state == ElgembelliasState::DEATH) {
                        return false; // don't hold, let play-state detect win
                    }
                }
            }

            return false;
        }

        // Check if Elgembellias is in DEATH state (win condition)
        bool isElgembelliasDead(World &world) {
            for (auto entity : world.getEntities()) {
                auto elgComp = entity->getComponent<ElgembelliasComponent>();
                if (elgComp && elgComp->state == ElgembelliasState::DEATH)
                    return true;
            }
            return false;
        }

        // Start a specific phase from phase-select menu
        void startPhase(int index, World &world, FireballSystem &fireballSystem) {
            switch (index) {
                case 0:
                    currentPhase = Phase::COOLDOWN;
                    break;
                case 1:
                    currentPhase = Phase::MARINES;
                    marinePhase.enter(&world, marinesConfig);
                    fireballSystem.grantDevilFruit(&world);
                    break;
                case 2:
                    currentPhase = Phase::OCTOPUS;
                    krakenPhase.enter(&world, krakenConfig);
                    fireballSystem.grantDevilFruit(&world);
                    break;
            }
            phaseTransitioning = false;
            std::cout << "[Phase] Starting from phase: " << index << std::endl;
        }
    };

} // namespace our
