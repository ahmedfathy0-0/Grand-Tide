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
#include <components/health.hpp>
#include <systems/fireball-system.hpp>

#include <asset-loader.hpp>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <iostream>
#include <json-utils.hpp>
#include <components/mesh-renderer.hpp>
#include <components/animator.hpp>
#include <miniaudio.h>

// Phase handlers
#include "shark-state.hpp"
#include "marine-state.hpp"
#include "kraken-state.hpp"

// This state shows how to use the ECS framework and deserialization.
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

    // Pause menu
    bool isPaused = false;
    int selectedMenuItem = 0; // 0=CONTINUE, 1=RESTART, 2=EXIT
    GLuint menuTextures[3] = {0, 0, 0};
    GLuint menuVAO = 0, menuVBO = 0;
    GLuint menuShader = 0;

    // Game phases
    // Start menu
    bool isStartMenu = true;
    int selectedStartItem = 0; // 0=START, 1=EXIT
    GLuint startTextures[2] = {0, 0};
    GLuint startVAO = 0, startVBO = 0;
    GLuint startShader = 0;

    // Game phases — delegated to phase handlers
    enum class GamePhase { SHARKS, MARINES, OCTOPUS };
    GamePhase currentPhase = GamePhase::SHARKS;
    bool phaseTransitioning = false;

    // Ocean background audio (looping)
    ma_decoder oceanDecoder;
    ma_device* oceanDevice = nullptr;
    bool oceanAudioReady = false;

    static void oceanAudioCallback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount) {
        auto* dec = static_cast<ma_decoder*>(pDevice->pUserData);
        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(dec, pOutput, frameCount, &framesRead);
        // Loop: if we didn't get enough frames, seek to start and read the rest
        if (framesRead < frameCount) {
            ma_decoder_seek_to_pcm_frame(dec, 0);
            ma_uint64 remaining = frameCount - framesRead;
            ma_uint64 framesRead2 = 0;
            ma_decoder_read_pcm_frames(dec, (ma_uint8*)pOutput + framesRead * dec->outputChannels * sizeof(float), remaining, &framesRead2);
        }
    }
    // Resource spawning
    int fishSpawned = 0;
    int woodSpawned = 0;
    float fishSpawnTimer = 0.0f;
    float woodSpawnTimer = 0.0f;
    float fishSpawnInterval = 5.0f;  // 5-8s randomized in update
    float woodSpawnInterval = 5.0f;  // 5-8s randomized in update
    int maxFishAlive = 5;
    int maxWoodAlive = 5;

    // Store original scales for entities we hide/show
    std::unordered_map<our::Entity*, glm::vec3> hiddenEntityScales;
    // Phase handler objects
    our::SharkPhase sharkPhase;
    our::MarinePhase marinePhase;
    our::KrakenPhase krakenPhase;

    // Phase configs loaded from JSON
    nlohmann::json sharksConfig;
    nlohmann::json marinesConfig;
    nlohmann::json krakenConfig;

    // Shared helpers for menu overlays
    static GLuint compileMenuShader(const char* vert, const char* frag) {
        auto compile = [](const char* src, GLenum type) -> GLuint {
            GLuint sh = glCreateShader(type);
            glShaderSource(sh, 1, &src, nullptr);
            glCompileShader(sh);
            int ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (!ok) { char buf[512]; glGetShaderInfoLog(sh, 512, nullptr, buf);
                std::cerr << "[Menu] shader error: " << buf << std::endl; glDeleteShader(sh); return 0; }
            return sh;
        };
        GLuint vs = compile(vert, GL_VERTEX_SHADER);
        GLuint fs = compile(frag, GL_FRAGMENT_SHADER);
        if (!vs || !fs) { if(vs) glDeleteShader(vs); if(fs) glDeleteShader(fs); return 0; }
        GLuint p = glCreateProgram();
        glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
        int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) { char buf[512]; glGetProgramInfoLog(p, 512, nullptr, buf);
            std::cerr << "[Menu] link error: " << buf << std::endl; glDeleteProgram(p); p = 0; }
        glDeleteShader(vs); glDeleteShader(fs);
        return p;
    }

    // Load a texture from file into a GL texture
    static GLuint loadTexture(const char* path) {
        int w, h, ch;
        unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
        if (!data) { std::cerr << "[Menu] Failed to load: " << path << std::endl; return 0; }
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    }

    // Create a fullscreen quad VAO
    static GLuint createQuadVAO(GLuint& vbo) {
        float quad[12] = { 0,0, 1,0, 1,1, 0,0, 1,1, 0,1 };
        GLuint vao;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
        return vao;
    }

    // Shared vertex shader for both menus
    static constexpr const char* menuVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = vec2(aPos.x, 1.0 - aPos.y);
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
}
)";
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

        // --- Load phase configs from JSON files ---
        sharksConfig = our::loadJsonFile("config/sharks-phase.json");
        marinesConfig = our::loadJsonFile("config/marines-phase.json");
        krakenConfig = our::loadJsonFile("config/kraken-phase.json");

        // Enter the first phase (SHARKS)
        currentPhase = GamePhase::SHARKS;
        phaseTransitioning = false;
        sharkPhase.enter(&world, sharksConfig);
        // We initialize the camera controller system since it needs a pointer to the app
        cameraController.enter(getApp());
        // Then we initialize the renderer
        auto size = getApp()->getFrameBufferSize();
        renderer.initialize(size, config["renderer"]);
        bossHealthBar.init(size.x, size.y);
        damageFlash.init();

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

        // --- Ocean Audio (looping) ---
        if (ma_decoder_init_file("assets/audios/ocean.mp3", nullptr, &oceanDecoder) == MA_SUCCESS) {
            oceanDevice = new ma_device();
            ma_device_config devCfg = ma_device_config_init(ma_device_type_playback);
            devCfg.playback.format = oceanDecoder.outputFormat;
            devCfg.playback.channels = oceanDecoder.outputChannels;
            devCfg.sampleRate = oceanDecoder.outputSampleRate;
            devCfg.dataCallback = oceanAudioCallback;
            devCfg.pUserData = &oceanDecoder;
            if (ma_device_init(nullptr, &devCfg, oceanDevice) == MA_SUCCESS) {
                ma_device_start(oceanDevice);
                oceanAudioReady = true;
                std::cerr << "[Play] Ocean audio started (looping)" << std::endl;
            } else {
                std::cerr << "[Play] Ocean audio device init failed" << std::endl;
                delete oceanDevice; oceanDevice = nullptr;
                ma_decoder_uninit(&oceanDecoder);
            }
        } else {
            std::cerr << "[Play] Could not load ocean.mp3" << std::endl;
        }

        // --- Pause Menu Initialization ---
        isPaused = false;
        selectedMenuItem = 0;
        stbi_set_flip_vertically_on_load(false);
        menuTextures[0] = loadTexture("assets/textures/menu_continue.png");
        menuTextures[1] = loadTexture("assets/textures/menu_restart.png");
        menuTextures[2] = loadTexture("assets/textures/menu_exit.png");

        static const char* menuFrag = R"(
#version 330 core
uniform sampler2D uMenuTex;
uniform float uAlpha;
in vec2 vUV;
out vec4 fragColor;
void main() {
    vec4 col = texture(uMenuTex, vUV);
    float brightness = (col.r + col.g + col.b) / 3.0;
    float darkMask = smoothstep(0.08, 0.20, brightness);
    fragColor = vec4(col.rgb, col.a * darkMask * uAlpha);
    if (fragColor.a < 0.02) discard;
}
)";
        menuShader = compileMenuShader(menuVertSrc, menuFrag);
        menuVAO = createQuadVAO(menuVBO);

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

    }

    void onDraw(double deltaTime) override
    {
        auto &keyboard = getApp()->getKeyboard();

        // --- Pause Menu Input (always checked, even when paused) ---
        if (keyboard.justPressed(GLFW_KEY_ESCAPE)) {
            isPaused = !isPaused;
            if (isPaused) selectedMenuItem = 0;
        }

        if (isPaused) {
            // Arrow key navigation
            if (keyboard.justPressed(GLFW_KEY_UP)) {
                selectedMenuItem = (selectedMenuItem - 1 + 3) % 3;
            }
            if (keyboard.justPressed(GLFW_KEY_DOWN)) {
                selectedMenuItem = (selectedMenuItem + 1) % 3;
            }
            if (keyboard.justPressed(GLFW_KEY_ENTER)) {
                if (selectedMenuItem == 0) {
                    isPaused = false;
                } else if (selectedMenuItem == 1) {
                    getApp()->changeState("play");
                    return;
                } else if (selectedMenuItem == 2) {
                    getApp()->close();
                    return;
                }
            }

            // Still render the scene (frozen) but skip all game logic
            renderer.render(&world);

            // Draw pause menu overlay on top
            if (menuShader && menuVAO && menuTextures[selectedMenuItem]) {
                glUseProgram(menuShader);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, menuTextures[selectedMenuItem]);
                glUniform1i(glGetUniformLocation(menuShader, "uMenuTex"), 0);
                glUniform1f(glGetUniformLocation(menuShader, "uAlpha"), 1.0f);

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);

                glBindVertexArray(menuVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                glEnable(GL_DEPTH_TEST);
                glDisable(GL_BLEND);
                glUseProgram(0);
            }
            return; // Freeze: skip all game logic
        }

        // --- Normal game update (not paused) ---

        // === SYSTEM UPDATES (always run, before phase logic) ===
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);
        survivalSystem.update();
        animationSystem.update(&world, (float)deltaTime);
        combatSystem.update(&world, (float)deltaTime);
        fireballSystem.update(&world, (float)deltaTime);

        // === PHASE MANAGEMENT — after systems so deaths are visible this frame ===
        if (currentPhase == GamePhase::SHARKS) {
            bool sharksDone = sharkPhase.update(&world, (float)deltaTime);
            // Transition to MARINES when devil fruit spawns (means all sharks are dead)
            bool fruitSpawned = fireballSystem.isDevilFruitSpawned();
            if (sharksDone && fruitSpawned && !phaseTransitioning) {
                phaseTransitioning = true;
                sharkPhase.exit();
                currentPhase = GamePhase::MARINES;
                marinePhase.enter(&world, marinesConfig);
                phaseTransitioning = false;
                std::cout << "[Phase] === DEVIL FRUIT SPAWNED === Transitioning to MARINES phase!" << std::endl;
            } else if (sharksDone && !fruitSpawned) {
                std::cout << "[Phase] Sharks done but fruit not spawned yet" << std::endl;
            }
        }
        else if (currentPhase == GamePhase::MARINES) {
            bool marinesDone = marinePhase.update(&world, (float)deltaTime);
            if (marinesDone && !phaseTransitioning) {
                phaseTransitioning = true;
                marinePhase.exit();
                currentPhase = GamePhase::OCTOPUS;
                krakenPhase.enter(&world, krakenConfig);
                phaseTransitioning = false;
                std::cout << "[Phase] === MARINES PHASE OVER === Transitioning to OCTOPUS phase!" << std::endl;
            }
        }
        else if (currentPhase == GamePhase::OCTOPUS) {
            bool krakenDone = krakenPhase.update(&world, (float)deltaTime, getApp());
            if (krakenDone) {
                krakenPhase.exit();
                std::cout << "[Phase] === KRAKEN DEFEATED === Game Over!" << std::endl;
                // TODO: victory screen
            }
        }

        // === RESOURCE SPAWNING & LIFETIME ===
        {
            int fishAlive = 0;
            int woodAlive = 0;
            for (auto entity : world.getEntities()) {
                auto res = entity->getComponent<our::ResourceComponent>();
                if (!res) continue;
                res->elapsed += (float)deltaTime;
                if (res->elapsed >= res->lifetime) {
                    std::cout << "[Expire] " << res->type << " expired at ("
                              << entity->localTransform.position.x << ","
                              << entity->localTransform.position.z << ")" << std::endl;
                    world.markForRemoval(entity);
                    continue;
                }
                if (res->type == "fish") fishAlive++;
                else if (res->type == "wood") woodAlive++;
            }

            fishSpawnTimer += (float)deltaTime;
            woodSpawnTimer += (float)deltaTime;

            if (fishAlive < maxFishAlive && fishSpawnTimer >= fishSpawnInterval) {
                fishSpawnTimer = 0.0f;
                fishSpawnInterval = 3.0f + static_cast<float>(rand() % 3); // 3-5s (faster)
                spawnFish(&world, raft);
            }
            if (woodAlive < maxWoodAlive && woodSpawnTimer >= woodSpawnInterval) {
                woodSpawnTimer = 0.0f;
                woodSpawnInterval = 6.0f + static_cast<float>(rand() % 4); // 6-9s (slower)
                spawnWood(&world, raft);
            }
        }

        world.deleteMarkedEntities();

        // And finally we use the renderer system to draw the scene
        renderer.render(&world);

        // Damage flash overlay (before HUD)
        // Check if player was just damaged and trigger red screen flash
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

        // Boss health bar (only in OCTOPUS phase)
        if (currentPhase == GamePhase::OCTOPUS) {
            auto size = getApp()->getFrameBufferSize();
            bossHealthBar.resize(size.x, size.y);
            krakenPhase.onGui(&world, bossHealthBar, (float)deltaTime);
        }

        // DEBUG: F1 deals 50 damage to octopus for enrage testing
        if (keyboard.justPressed(GLFW_KEY_F1))
        {
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
        // Don't forget to destroy the renderer
        renderer.destroy();
        bossHealthBar.destroy();
        damageFlash.destroy();
        // Pause menu cleanup
        for (int i = 0; i < 3; i++) {
            if (menuTextures[i]) glDeleteTextures(1, &menuTextures[i]);
        }
        if (menuVAO) glDeleteVertexArrays(1, &menuVAO);
        if (menuVBO) glDeleteBuffers(1, &menuVBO);
        if (menuShader) glDeleteProgram(menuShader);
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Ocean audio cleanup
        if (oceanDevice) {
            ma_device_stop(oceanDevice);
            ma_device_uninit(oceanDevice);
            delete oceanDevice; oceanDevice = nullptr;
        }
        if (oceanAudioReady) { ma_decoder_uninit(&oceanDecoder); oceanAudioReady = false; }
        // Clear the world
        world.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};

