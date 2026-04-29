#pragma once

#include <application.hpp>

#include <ecs/world.hpp>
#include <systems/forward-renderer.hpp>
#include <systems/player-controller-system.hpp>
#include <systems/movement.hpp>
#include <systems/survival-system.hpp>
#include <systems/animation-system.hpp>
#include <systems/octopus-system.hpp>
#include <systems/combat-system.hpp>
#include <systems/marine-boat-system.hpp>
#include <systems/boss-health-bar.hpp>
#include <systems/damage-flash.hpp>
#include <components/health.hpp>
#include <systems/fireball-system.hpp>

#include <asset-loader.hpp>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <iostream>
#include <unordered_map>
#include <components/shark-component.hpp>
#include <components/enemy.hpp>
#include <components/octopus-component.hpp>
#include <components/musket-component.hpp>
#include <components/marine-boat-component.hpp>
#include <components/mesh-renderer.hpp>
#include <components/animator.hpp>
#include <mesh/model.hpp>

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
    our::OctopusSystem octopusSystem;
    our::BossHealthBar bossHealthBar;
    our::DamageFlash damageFlash;
    our::FireballSystem fireballSystem;

    // Pause menu
    bool isPaused = false;
    int selectedMenuItem = 0; // 0=CONTINUE, 1=RESTART, 2=EXIT
    GLuint menuTextures[3] = {0, 0, 0};
    GLuint menuVAO = 0, menuVBO = 0;
    GLuint menuShader = 0;

    // Start menu
    bool isStartMenu = true;
    int selectedStartItem = 0; // 0=START, 1=EXIT
    GLuint startTextures[2] = {0, 0};
    GLuint startVAO = 0, startVBO = 0;
    GLuint startShader = 0;

    // Game phases
    enum class GamePhase { SHARKS, MARINES, OCTOPUS };
    GamePhase currentPhase = GamePhase::SHARKS;
    float phaseTimer = 0.0f;          // Used in MARINES phase for 5-min survival timer
    int sharksKilled = 0;
    int sharksToKill = 1;
    int sharksSpawned = 0;
    float sharkSpawnTimer = 0.0f;
    float sharkSpawnInterval = 8.0f;   // Seconds between shark spawns
    bool phaseTransitioning = false;   // Prevent double-transition

    // Store original scales for entities we hide/show
    std::unordered_map<our::Entity*, glm::vec3> hiddenEntityScales;


    void hideEntity(our::Entity* e) {
        if (e && hiddenEntityScales.find(e) == hiddenEntityScales.end()) {
            hiddenEntityScales[e] = e->localTransform.scale;
            e->localTransform.scale = glm::vec3(0.0f);
        }
    }
    void showEntity(our::Entity* e) {
        auto it = hiddenEntityScales.find(e);
        if (it != hiddenEntityScales.end()) {
            e->localTransform.scale = it->second;
            hiddenEntityScales.erase(it);
        } else if (e) {
            // Entity was never hidden via our system but scale might be 0 — force restore from config
            // This handles entities spawned by MarineBoatSystem that weren't in hiddenEntityScales
            if (e->localTransform.scale.x == 0.0f && e->localTransform.scale.y == 0.0f && e->localTransform.scale.z == 0.0f) {
                // Check if it has a MarineBoatComponent — default scale is 0.05
                if (e->getComponent<our::MarineBoatComponent>()) {
                    e->localTransform.scale = glm::vec3(0.05f);
                    std::cout << "[Phase] Force-restored scale for boat: " << e->name << std::endl;
                } else if (e->getComponent<our::MusketComponent>()) {
                    // Musket children have varying scales, try 50.0 as default
                    if (e->parent) {
                        e->localTransform.scale = glm::vec3(50.0f);
                        std::cout << "[Phase] Force-restored scale for musket child: " << e->name << std::endl;
                    } else {
                        e->localTransform.scale = glm::vec3(2.0f);
                        std::cout << "[Phase] Force-restored scale for standalone musket: " << e->name << std::endl;
                    }
                }
            }
        }
    }
    void hideEntityTree(our::Entity* e, our::World* w) {
        hideEntity(e);
        for (auto entity : w->getEntities()) {
            if (entity->parent == e) hideEntityTree(entity, w);
        }
    }
    void showEntityTree(our::Entity* e, our::World* w) {
        showEntity(e);
        for (auto entity : w->getEntities()) {
            if (entity->parent == e) showEntityTree(entity, w);
        }
    }

    void spawnShark(our::World* world, our::Entity* raft) {
        if (!raft) return;
        glm::vec3 raftPos = raft->localTransform.position;
        float angle = static_cast<float>(rand() % 360) * glm::pi<float>() / 180.0f;
        float spawnDist = 150.0f + static_cast<float>(rand() % 100);

        our::Entity* shark = world->add();
        shark->name = "shark_" + std::to_string(sharksSpawned);
        shark->localTransform.position = glm::vec3(
            raftPos.x + cos(angle) * spawnDist,
            -10.0f,
            raftPos.z + sin(angle) * spawnDist);
        shark->localTransform.scale = glm::vec3(0.05f);
        shark->localTransform.rotation = glm::vec3(0.0f);

        auto* mr = shark->addComponent<our::MeshRendererComponent>();
        mr->mesh = nullptr;
        mr->material = our::AssetLoader<our::Material>::get("lit_shark");

        auto* sc = shark->addComponent<our::SharkComponent>();
        sc->health = 100.0f;
        sc->state = our::SharkState::APPROACHING;
        sc->speed = 12.0f + static_cast<float>(rand() % 6); // 12-18 speed variety
        sc->attackRange = 25.0f;

        auto* anim = shark->addComponent<our::AnimatorComponent>();
        anim->modelName = "shark";
        anim->currentAnimIndex = 0;

        auto* enemy = shark->addComponent<our::EnemyComponent>();
        enemy->type = our::EnemyType::SHARK;
        enemy->primaryTarget = our::PrimaryTarget::RAFT;
        enemy->state = our::EnemyState::ALIVE;
        enemy->attackDamage = 100.0f;

        auto* hp = shark->addComponent<our::HealthComponent>();
        hp->maxHealth = 100.0f;
        hp->currentHealth = 100.0f;

        sharksSpawned++;
        std::cout << "[Phase] Spawned shark #" << sharksSpawned << " at ("
                  << shark->localTransform.position.x << ","
                  << shark->localTransform.position.z << ")" << std::endl;
    }

    // Menu shader helper
    static GLuint compileMenuShader(const char* vert, const char* frag) {
        auto compile = [](const char* src, GLenum type) -> GLuint {
            GLuint sh = glCreateShader(type);
            glShaderSource(sh, 1, &src, nullptr);
            glCompileShader(sh);
            int ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (!ok) { char buf[512]; glGetShaderInfoLog(sh, 512, nullptr, buf);
                std::cerr << "[PauseMenu] shader error: " << buf << std::endl; glDeleteShader(sh); return 0; }
            return sh;
        };
        GLuint vs = compile(vert, GL_VERTEX_SHADER);
        GLuint fs = compile(frag, GL_FRAGMENT_SHADER);
        if (!vs || !fs) { if(vs) glDeleteShader(vs); if(fs) glDeleteShader(fs); return 0; }
        GLuint p = glCreateProgram();
        glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
        int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) { char buf[512]; glGetProgramInfoLog(p, 512, nullptr, buf);
            std::cerr << "[PauseMenu] link error: " << buf << std::endl; glDeleteProgram(p); p = 0; }
        glDeleteShader(vs); glDeleteShader(fs);
        return p;
    }
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

        // --- Pause Menu Initialization ---
        isPaused = false;
        selectedMenuItem = 0;

        // Load 3 menu textures
        stbi_set_flip_vertically_on_load(false);
        const char* menuPaths[3] = {
            "assets/textures/menu_continue.png",
            "assets/textures/menu_restart.png",
            "assets/textures/menu_exit.png"
        };
        for (int i = 0; i < 3; i++) {
            int w, h, ch;
            unsigned char* data = stbi_load(menuPaths[i], &w, &h, &ch, 4);
            if (data) {
                glGenTextures(1, &menuTextures[i]);
                glBindTexture(GL_TEXTURE_2D, menuTextures[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                stbi_image_free(data);
                glBindTexture(GL_TEXTURE_2D, 0);
            } else {
                std::cerr << "[PauseMenu] Failed to load: " << menuPaths[i] << std::endl;
            }
        }

        // Compile menu shader
        static const char* menuVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = vec2(aPos.x, 1.0 - aPos.y);
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
}
)";
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
        menuShader = compileMenuShader(menuVert, menuFrag);

        // Create fullscreen quad VAO/VBO
        float menuQuad[12] = {
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
            0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f
        };
        glGenVertexArrays(1, &menuVAO);
        glGenBuffers(1, &menuVBO);
        glBindVertexArray(menuVAO);
        glBindBuffer(GL_ARRAY_BUFFER, menuVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(menuQuad), menuQuad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
        fireballSystem.enter(getApp());

        // --- Start Menu Initialization ---
        isStartMenu = true;
        selectedStartItem = 0;

        // Load 2 start menu textures
        const char* startPaths[2] = {
            "assets/textures/game_play.png",
            "assets/textures/game_exit.png"
        };
        for (int i = 0; i < 2; i++) {
            int w, h, ch;
            unsigned char* data = stbi_load(startPaths[i], &w, &h, &ch, 4);
            if (data) {
                glGenTextures(1, &startTextures[i]);
                glBindTexture(GL_TEXTURE_2D, startTextures[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                stbi_image_free(data);
                glBindTexture(GL_TEXTURE_2D, 0);
            } else {
                std::cerr << "[StartMenu] Failed to load: " << startPaths[i] << std::endl;
            }
        }

        // Compile start menu shader (no transparency, UV flipped on Y)
        static const char* startVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = vec2(aPos.x, 1.0 - aPos.y);
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
}
)";
        static const char* startFrag = R"(
#version 330 core
uniform sampler2D uStartTex;
in vec2 vUV;
out vec4 fragColor;
void main() {
    vec4 col = texture(uStartTex, vUV);
    fragColor = vec4(col.rgb, 1.0);
}
)";
        startShader = compileMenuShader(startVert, startFrag);

        // Create fullscreen quad VAO/VBO for start menu
        float startQuad[12] = {
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
            0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f
        };
        glGenVertexArrays(1, &startVAO);
        glGenBuffers(1, &startVBO);
        glBindVertexArray(startVAO);
        glBindBuffer(GL_ARRAY_BUFFER, startVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(startQuad), startQuad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);

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
            float remaining = 300.0f - phaseTimer;
            if (remaining < 0.0f) remaining = 0.0f;
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

        // --- Start Menu (shown at game launch) ---
        if (isStartMenu) {
            if (keyboard.justPressed(GLFW_KEY_UP)) {
                selectedStartItem = (selectedStartItem - 1 + 2) % 2;
            }
            if (keyboard.justPressed(GLFW_KEY_DOWN)) {
                selectedStartItem = (selectedStartItem + 1) % 2;
            }
            if (keyboard.justPressed(GLFW_KEY_ENTER)) {
                if (selectedStartItem == 0) {
                    isStartMenu = false; // Start game
                } else if (selectedStartItem == 1) {
                    getApp()->close();
                    return;
                }
            }

            // Clear screen with black
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Draw start menu fullscreen
            if (startShader && startVAO && startTextures[selectedStartItem]) {
                glUseProgram(startShader);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, startTextures[selectedStartItem]);
                glUniform1i(glGetUniformLocation(startShader, "uStartTex"), 0);

                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                glDisable(GL_BLEND);

                glBindVertexArray(startVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                glEnable(GL_DEPTH_TEST);
                glUseProgram(0);
            }
            return; // Don't run any game logic
        }

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

        // === PHASE MANAGEMENT ===
        our::Entity* raft = nullptr;
        our::Entity* player = nullptr;
        for (auto entity : world.getEntities()) {
            if (entity->name == "raft") raft = entity;
            if (entity->name == "player") player = entity;
        }

        if (currentPhase == GamePhase::SHARKS) {
            // Hide octopus and all marines
            for (auto entity : world.getEntities()) {
                if (entity->getComponent<our::OctopusComponent>()) hideEntity(entity);
                if (entity->getComponent<our::MarineBoatComponent>()) hideEntityTree(entity, &world);
                if (entity->name == "debug_marine") hideEntity(entity);
                if (entity->getComponent<our::MusketComponent>() && !entity->parent) hideEntity(entity);
            }

            // Spawn sharks until we have enough
            if (sharksSpawned < sharksToKill) {
                sharkSpawnTimer += (float)deltaTime;
                if (sharkSpawnTimer >= sharkSpawnInterval) {
                    sharkSpawnTimer = 0.0f;
                    spawnShark(&world, raft);
                }
            }
        }
        else if (currentPhase == GamePhase::MARINES) {
            // Show marines, hide octopus
            for (auto entity : world.getEntities()) {
                if (entity->getComponent<our::OctopusComponent>()) hideEntity(entity);
                if (entity->getComponent<our::MarineBoatComponent>()) showEntityTree(entity, &world);
                if (entity->name == "debug_marine") showEntity(entity);
                if (entity->getComponent<our::MusketComponent>() && !entity->parent) showEntity(entity);
            }

            phaseTimer += (float)deltaTime;

            // Check if all marines are dead (only if at least one boat has spawned)
            bool anyMarineAlive = false;
            bool anyMarineExists = false;
            for (auto entity : world.getEntities()) {
                auto boat = entity->getComponent<our::MarineBoatComponent>();
                auto musket = entity->getComponent<our::MusketComponent>();
                auto enemy = entity->getComponent<our::EnemyComponent>();
                if (boat || musket) anyMarineExists = true;
                if ((boat || musket) && enemy && enemy->state != our::EnemyState::DEAD) {
                    anyMarineAlive = true;
                }
            }

            // Transition: survived 5 minutes, OR all marines dead (but only if marines have spawned)
            if ((phaseTimer >= 300.0f || (anyMarineExists && !anyMarineAlive)) && !phaseTransitioning) {
                phaseTransitioning = true;
                currentPhase = GamePhase::OCTOPUS;
                std::cout << "[Phase] === MARINES PHASE OVER === ("
                          << (anyMarineAlive ? "survived 5 min" : "all marines killed")
                          << ") Transitioning to OCTOPUS phase!" << std::endl;
                // If marines survived, they stay. If all dead, nothing to do.
            }
        }
        else if (currentPhase == GamePhase::OCTOPUS) {
            // Show octopus, keep marines as-is (visible if they survived)
            for (auto entity : world.getEntities()) {
                if (entity->getComponent<our::OctopusComponent>()) showEntity(entity);
            }
        }

        // === SYSTEM UPDATES (phase-aware) ===
        movementSystem.update(&world, (float)deltaTime);
        cameraController.update(&world, (float)deltaTime);
        survivalSystem.update();
        animationSystem.update(&world, (float)deltaTime);
        combatSystem.update(&world, (float)deltaTime);
        fireballSystem.update(&world, (float)deltaTime);

        // Only run marine boat system in MARINES and OCTOPUS phases
        if (currentPhase == GamePhase::MARINES || currentPhase == GamePhase::OCTOPUS) {
            marineBoatSystem.update(&world, (float)deltaTime);
        }

        // Only run octopus system in OCTOPUS phase
        if (currentPhase == GamePhase::OCTOPUS) {
            octopusSystem.update(&world, (float)deltaTime, getApp());
        }

        // === POST-SYSTEM PHASE CHECKS (after combat, before entity deletion) ===
        if (currentPhase == GamePhase::SHARKS) {
            // Transition to MARINES when devil fruit spawns (means all sharks are dead)
            if (fireballSystem.isDevilFruitSpawned() && !phaseTransitioning) {
                phaseTransitioning = true;
                currentPhase = GamePhase::MARINES;
                phaseTimer = 0.0f;
                marineBoatSystem = our::MarineBoatSystem();
                phaseTransitioning = false;  // Allow next phase transition
                std::cout << "[Phase] === DEVIL FRUIT SPAWNED === Transitioning to MARINES phase!" << std::endl;
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

        // Find octopus health and draw boss bar (only in OCTOPUS phase)
        if (currentPhase == GamePhase::OCTOPUS) {
        our::HealthComponent* octopusHealth = nullptr;
        our::OctopusComponent* octopusComp = nullptr;
        for (auto entity : world.getEntities()) {
            if (entity->name == "octopus") {
                octopusHealth = entity->getComponent<our::HealthComponent>();
                octopusComp = entity->getComponent<our::OctopusComponent>();
                break;
            }
        }
        if (octopusHealth && octopusComp && !octopusComp->permanentlyDead) {
            auto size = getApp()->getFrameBufferSize();
            bossHealthBar.setRevived(octopusComp->hasRevived);
            bossHealthBar.resize(size.x, size.y);
            bossHealthBar.update(octopusHealth->currentHealth, octopusHealth->maxHealth, (float)deltaTime);
            bossHealthBar.render();
        }
        } // end if (currentPhase == GamePhase::OCTOPUS)

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
        // Start menu cleanup
        for (int i = 0; i < 2; i++) {
            if (startTextures[i]) glDeleteTextures(1, &startTextures[i]);
        }
        if (startVAO) glDeleteVertexArrays(1, &startVAO);
        if (startVBO) glDeleteBuffers(1, &startVBO);
        if (startShader) glDeleteProgram(startShader);
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Clear the world
        world.clear();
        // and we delete all the loaded assets to free memory on the RAM and the VRAM
        our::clearAllAssets();
    }
};

// Force build 2
