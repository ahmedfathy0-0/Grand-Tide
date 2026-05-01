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
#include <components/elgembellias-component.hpp>
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
    our::ElgembelliasSystem elgembelliasSystem;
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

    // Loading screen (shown after Play, before phase select)
    bool isLoading = false;
    bool loadingDone = false;
    int loadingStep = -1; // -1=display only, 0=assets, 1=world, 2=renderer, 3=done
    float loadingProgress = 0.0f;
    GLuint loadingTexture = 0;
    GLuint loadingVAO = 0, loadingVBO = 0;
    GLuint loadingShader = 0;

    // Phase selection menu (after pressing Play)
    bool isPhaseSelectMenu = false;
    int selectedPhaseIndex = 0; // 0=SHARKS, 1=MARINES, 2=OCTOPUS
    GLuint phaseTextures[3] = {0, 0, 0};
    GLuint phaseVAO = 0, phaseVBO = 0;
    GLuint phaseShader = 0;
    GLuint arrowTexture = 0; // arrow indicator texture

    // Game phases
    enum class GamePhase { SHARKS, MARINES, OCTOPUS, ELGEMBELLIAS };
    GamePhase currentPhase = GamePhase::SHARKS;
    float phaseTimer = 0.0f;          // Used in MARINES phase for 5-min survival timer
    int sharksKilled = 0;
    int sharksToKill = 1;
    int sharksSpawned = 0;
    float sharkSpawnTimer = 0.0f;
    float sharkSpawnInterval = 8.0f;   // Seconds between shark spawns
    bool phaseTransitioning = false;   // Prevent double-transition

    // Track whether assets have been loaded (skip re-loading on restart for speed)
    static inline bool assetsLoaded = false;
    static inline bool rendererInitialized = false;

    // Elgembellias phase tracking
    bool elgembelliasSpawned = false;

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
        // Reset all game state on restart
        currentPhase = GamePhase::SHARKS;
        phaseTimer = 0.0f;
        sharksKilled = 0;
        sharksSpawned = 0;
        sharkSpawnTimer = 0.0f;
        phaseTransitioning = false;
        elgembelliasSpawned = false;
        isPaused = false;
        isStartMenu = true;
        isLoading = false;
        loadingDone = false;
        loadingStep = -1;
        loadingProgress = 0.0f;
        isPhaseSelectMenu = false;
        selectedStartItem = 0;
        selectedPhaseIndex = 0;
        hiddenEntityScales.clear();
        fireballSystem.reset();
        cameraController.reset();

        // NOTE: Heavy loading (assets, world, renderer) is deferred to the loading screen.
        // Both first launch and restart go through the loading screen.

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

        // --- Phase Selection Menu Initialization ---
        isPhaseSelectMenu = false;
        selectedPhaseIndex = 0;

        // Load 3 phase textures
        const char* phasePaths[3] = {
            "assets/textures/sharks_phase.png",
            "assets/textures/marine_phase.png",
            "assets/textures/kraken_phase.png"
        };
        for (int i = 0; i < 3; i++) {
            int w, h, ch;
            unsigned char* data = stbi_load(phasePaths[i], &w, &h, &ch, 4);
            if (data) {
                glGenTextures(1, &phaseTextures[i]);
                glBindTexture(GL_TEXTURE_2D, phaseTextures[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                stbi_image_free(data);
                glBindTexture(GL_TEXTURE_2D, 0);
            } else {
                std::cerr << "[PhaseSelect] Failed to load: " << phasePaths[i] << std::endl;
            }
        }

        // Phase selection shader (supports transparency + highlight tint)
        static const char* phaseVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uOffset;
uniform vec2 uScale;
out vec2 vUV;
void main() {
    vUV = vec2(aPos.x, 1.0 - aPos.y);
    vec2 pos = aPos * uScale + uOffset;
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
)";
        static const char* phaseFrag = R"(
#version 330 core
uniform sampler2D uPhaseTex;
uniform float uHighlight;  // 1.0 = selected (bright), 0.0 = dimmed
in vec2 vUV;
out vec4 fragColor;
void main() {
    vec4 col = texture(uPhaseTex, vUV);
    if (col.a < 0.02) discard;
    // Dim unselected items, brighten selected
    float brightness = mix(0.4, 1.0, uHighlight);
    fragColor = vec4(col.rgb * brightness, col.a);
}
)";
        phaseShader = compileMenuShader(phaseVert, phaseFrag);

        // Create quad VAO/VBO for phase selection (same quad geometry)
        float phaseQuad[12] = {
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
            0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f
        };
        glGenVertexArrays(1, &phaseVAO);
        glGenBuffers(1, &phaseVBO);
        glBindVertexArray(phaseVAO);
        glBindBuffer(GL_ARRAY_BUFFER, phaseVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(phaseQuad), phaseQuad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);

        // --- Loading Screen Initialization ---
        {
            int w, h, ch;
            unsigned char* data = stbi_load("assets/textures/loading.png", &w, &h, &ch, 4);
            if (data) {
                glGenTextures(1, &loadingTexture);
                glBindTexture(GL_TEXTURE_2D, loadingTexture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                stbi_image_free(data);
                glBindTexture(GL_TEXTURE_2D, 0);
            } else {
                std::cerr << "[LoadingScreen] Failed to load loading.png" << std::endl;
            }

            static const char* loadingVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = vec2(aPos.x, 1.0 - aPos.y);
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
}
)";
            static const char* loadingFrag = R"(
#version 330 core
uniform sampler2D uLoadingTex;
in vec2 vUV;
out vec4 fragColor;
void main() {
    fragColor = texture(uLoadingTex, vUV);
}
)";
            loadingShader = compileMenuShader(loadingVert, loadingFrag);

            float loadingQuad[12] = {
                0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
                0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f
            };
            glGenVertexArrays(1, &loadingVAO);
            glGenBuffers(1, &loadingVBO);
            glBindVertexArray(loadingVAO);
            glBindBuffer(GL_ARRAY_BUFFER, loadingVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(loadingQuad), loadingQuad, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
            glBindVertexArray(0);
        }


    }

    void onImmediateGui() override
    {
        // --- Loading Screen Overlay ---
        if (isLoading && !loadingDone) {
            return; // Don't draw game UI while loading
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
        if (currentPhase == GamePhase::MARINES && !isPaused) {
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

        // --- Phase Selection Menu ImGui overlay ---
        if (isPhaseSelectMenu) {
            auto size = getApp()->getFrameBufferSize();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

            // Title
            ImGui::SetNextWindowPos(ImVec2(size.x * 0.5f, size.y * 0.08f), 0, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("PhaseSelectTitle", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("SELECT STARTING PHASE");
            ImGui::End();

            // Compute card positions
            float cardW_px = size.x * 0.20f;
            float gap_px = size.x * 0.05f;
            float totalW_px = 3.0f * cardW_px + 2.0f * gap_px;
            float startX_px = size.x * 0.5f - totalW_px / 2.0f;
            float selCenterX = startX_px + selectedPhaseIndex * (cardW_px + gap_px) + cardW_px * 0.5f;
            float cardTopY = size.y * 0.5f - size.y * 0.25f;

            // Left arrow
            ImGui::SetNextWindowPos(ImVec2(selCenterX - cardW_px * 0.6f, cardTopY + size.y * 0.25f * 0.5f - 15), 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("ArrowLeft", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("<<");
            ImGui::End();

            // Right arrow
            ImGui::SetNextWindowPos(ImVec2(selCenterX + cardW_px * 0.6f, cardTopY + size.y * 0.25f * 0.5f - 15), 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("ArrowRight", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text(">>");
            ImGui::End();

            // Phase name under selected card
            const char* phaseNames[3] = {"SHARKS", "MARINES", "KRAKEN"};
            ImGui::SetNextWindowPos(ImVec2(selCenterX, cardTopY + size.y * 0.25f + 20), 0, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("PhaseName", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("%s", phaseNames[selectedPhaseIndex]);
            ImGui::End();

            // Instructions
            ImGui::SetNextWindowPos(ImVec2(size.x * 0.5f, size.y * 0.92f), 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("PhaseSelectHelp", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("LEFT/RIGHT to select  |  ENTER to start  |  ESC to go back");
            ImGui::End();

            ImGui::PopStyleColor();
        }

    }

    void onDraw(double deltaTime) override
    {
        // Cap deltaTime to prevent huge jumps after unpausing or menu transitions
   
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
                    isStartMenu = false;
                    isLoading = true; // Show loading screen (fast on restart since assets already loaded)
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

        // --- Loading Screen (after pressing Play, before phase select) ---
        if (isLoading && !loadingDone) {
            auto &config = getApp()->getConfig()["scene"];

            // Step -1: First frame - just display loading image, no heavy work yet
            if (loadingStep == -1) {
                loadingProgress = 0.0f;
                loadingStep = 0; // Next frame will start actual loading
            }
            // Step 0: Load assets (shaders, textures, meshes, models, materials)
            else if (loadingStep == 0) {
                loadingProgress = 0.1f;
                if (!assetsLoaded) {
                    if (config.contains("assets"))
                        our::deserializeAllAssets(config["assets"]);
                    assetsLoaded = true;
                }
                loadingStep = 1;
            }
            // Step 1: Deserialize world entities
            else if (loadingStep == 1) {
                loadingProgress = 0.5f;
                world.clear(); // Clear old entities before re-deserializing
                if (config.contains("world"))
                    world.deserialize(config["world"]);
                cameraController.enter(getApp());
                loadingStep = 2;
            }
            // Step 2: Initialize renderer + UI
            else if (loadingStep == 2) {
                loadingProgress = 0.8f;
                auto size = getApp()->getFrameBufferSize();
                if (!rendererInitialized) {
                    renderer.initialize(size, config["renderer"]);
                    rendererInitialized = true;
                }
                bossHealthBar.init(size.x, size.y);
                damageFlash.init();
                our::Entity *player = nullptr;
                our::Entity *boat = nullptr;
                for (auto entity : world.getEntities()) {
                    if (entity->name == "player") player = entity;
                    if (entity->name == "raft") boat = entity;
                }
                survivalSystem.setup(&world, getApp(), player, boat);
                fireballSystem.enter(getApp());
                loadingStep = 3;
            }
            // Step 3: Done - transition to phase select
            else if (loadingStep == 3) {
                loadingProgress = 1.0f;
                loadingDone = true;
                isLoading = false;
                isPhaseSelectMenu = true;
            }

            // Draw loading screen background (loading.png fullscreen)
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (loadingShader && loadingVAO && loadingTexture) {
                glUseProgram(loadingShader);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, loadingTexture);
                glUniform1i(glGetUniformLocation(loadingShader, "uLoadingTex"), 0);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                glDisable(GL_BLEND);
                glBindVertexArray(loadingVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
                glEnable(GL_DEPTH_TEST);
                glUseProgram(0);
            }
            return; // Don't run game logic while loading
        }

        // --- Phase Selection Menu (after pressing Play) ---
        if (isPhaseSelectMenu) {
            if (keyboard.justPressed(GLFW_KEY_LEFT)) {
                selectedPhaseIndex = (selectedPhaseIndex - 1 + 3) % 3;
            }
            if (keyboard.justPressed(GLFW_KEY_RIGHT)) {
                selectedPhaseIndex = (selectedPhaseIndex + 1) % 3;
            }
            if (keyboard.justPressed(GLFW_KEY_ENTER)) {
                isPhaseSelectMenu = false;
                // Set starting phase based on selection
                switch (selectedPhaseIndex) {
                    case 0: currentPhase = GamePhase::SHARKS; break;
                    case 1:
                        currentPhase = GamePhase::MARINES;
                        marineBoatSystem = our::MarineBoatSystem(); // Initialize marine spawner
                        fireballSystem.grantDevilFruit(&world); // Give devil fruit directly
                        break;
                    case 2:
                        currentPhase = GamePhase::OCTOPUS;
                        fireballSystem.grantDevilFruit(&world); // Give devil fruit directly
                        break;
                }
                phaseTimer = 0.0f;
                phaseTransitioning = false;
                std::cout << "[Phase] Starting from phase: " << selectedPhaseIndex << std::endl;
            }
            if (keyboard.justPressed(GLFW_KEY_ESCAPE)) {
                isPhaseSelectMenu = false;
                isStartMenu = true; // Go back to start menu
            }

            // Clear screen with dark background
            glClearColor(0.05f, 0.05f, 0.1f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Draw 3 phase images in a horizontal row
            if (phaseShader && phaseVAO) {
                glUseProgram(phaseShader);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                // Each card: 20% of screen width, 50% of screen height
                // Centered vertically, spaced evenly across center
                float cardW = 0.38f;  // width in NDC (0..1)
                float cardH = 0.38f;  // height in NDC (0..1)
                float gap = 0.0005f;    // gap between cards
                float totalW = 3.0f * cardW + 2.0f * gap;
                float startX = 0.5f - totalW / 2.0f;  // center horizontally
                float startY = 0.5f - cardH / 2.0f;    // center vertically

                GLint offsetLoc = glGetUniformLocation(phaseShader, "uOffset");
                GLint scaleLoc = glGetUniformLocation(phaseShader, "uScale");
                GLint texLoc = glGetUniformLocation(phaseShader, "uPhaseTex");
                GLint highlightLoc = glGetUniformLocation(phaseShader, "uHighlight");

                glUniform1i(texLoc, 0);

                for (int i = 0; i < 3; i++) {
                    float x = startX + i * (cardW + gap);
                    float y = startY;
                    float highlight = (i == selectedPhaseIndex) ? 1.0f : 0.0f;

                    glUniform2f(offsetLoc, x, y);
                    glUniform2f(scaleLoc, cardW, cardH);
                    glUniform1f(highlightLoc, highlight);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, phaseTextures[i]);

                    glBindVertexArray(phaseVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }

                glBindVertexArray(0);
                glDisable(GL_BLEND);
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
                    // Restart: go back to start menu, loading happens when Play is pressed
                    isPaused = false;
                    isStartMenu = true;
                    selectedStartItem = 0;
                    // Reset game state but keep assets/renderer loaded
                    currentPhase = GamePhase::SHARKS;
                    phaseTimer = 0.0f;
                    sharksKilled = 0;
                    sharksSpawned = 0;
                    sharkSpawnTimer = 0.0f;
                    phaseTransitioning = false;
                    elgembelliasSpawned = false;
                    isPhaseSelectMenu = false;
                    selectedPhaseIndex = 0;
                    hiddenEntityScales.clear();
                    fireballSystem.reset();
                    cameraController.reset();
                    isLoading = false;
                    loadingDone = false;
                    loadingStep = -1;
                    loadingProgress = 0.0f;
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
            // Show octopus, hide all marines (even if they survived from a MARINES phase)
            for (auto entity : world.getEntities()) {
                if (entity->getComponent<our::OctopusComponent>()) showEntity(entity);
                if (entity->getComponent<our::MarineBoatComponent>()) hideEntityTree(entity, &world);
                if (entity->name == "debug_marine") hideEntity(entity);
                if (entity->getComponent<our::MusketComponent>() && !entity->parent) hideEntity(entity);
            }
        }
        else if (currentPhase == GamePhase::ELGEMBELLIAS) {
            // Show Elgembellias, keep dead octopus visible
            for (auto entity : world.getEntities()) {
                if (entity->getComponent<our::ElgembelliasComponent>()) showEntity(entity);
            }
        }

        // === SYSTEM UPDATES (phase-aware) ===
    movementSystem.update(&world, (float)deltaTime);
    cameraController.update(&world, (float)deltaTime);
        survivalSystem.update();
        animationSystem.update(&world, (float)deltaTime);
        combatSystem.update(&world, (float)deltaTime);
        fireballSystem.update(&world, (float)deltaTime);

        // Only run marine boat system in MARINES phase (not OCTOPUS - no new marines spawn then)
        if (currentPhase == GamePhase::MARINES) {
            marineBoatSystem.update(&world, (float)deltaTime);
        }

        // Only run octopus system in OCTOPUS phase
        if (currentPhase == GamePhase::OCTOPUS) {
            octopusSystem.update(&world, (float)deltaTime, getApp());
        }

        // Only run elgembellias system in ELGEMBELLIAS phase
        if (currentPhase == GamePhase::ELGEMBELLIAS) {
            elgembelliasSystem.update(&world, (float)deltaTime, player);
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
        else if (currentPhase == GamePhase::OCTOPUS) {
            // Check if octopus is permanently dead -> transition to ELGEMBELLIAS phase
            for (auto entity : world.getEntities()) {
                auto octComp = entity->getComponent<our::OctopusComponent>();
                if (octComp && octComp->permanentlyDead && !phaseTransitioning) {
                    phaseTransitioning = true;
                    currentPhase = GamePhase::ELGEMBELLIAS;
                    elgembelliasSpawned = false;
                    phaseTransitioning = false;
                    std::cout << "[Phase] === OCTOPUS PERMANENTLY DEAD === Transitioning to ELGEMBELLIAS phase!" << std::endl;
                    break;
                }
            }
        }
        else if (currentPhase == GamePhase::ELGEMBELLIAS) {
            // Spawn Elgembellias entity if not yet spawned (using the system)
            if (!elgembelliasSpawned && raft) {
                // Find octopus death position to avoid spawning there
                glm::vec3 octopusDeathPos = raft->localTransform.position;
                for (auto entity : world.getEntities()) {
                    if (entity->getComponent<our::OctopusComponent>()) {
                        octopusDeathPos = entity->localTransform.position;
                        break;
                    }
                }
                elgembelliasSystem.spawn(&world, raft, octopusDeathPos);
                elgembelliasSpawned = true;
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
        // Don't destroy renderer on restart - it's expensive and reusable
        // Only destroy UI-specific resources that get recreated in onEnter/onInitialize
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
        // Phase selection menu cleanup
        for (int i = 0; i < 3; i++) {
            if (phaseTextures[i]) glDeleteTextures(1, &phaseTextures[i]);
        }
        if (phaseVAO) glDeleteVertexArrays(1, &phaseVAO);
        if (phaseVBO) glDeleteBuffers(1, &phaseVBO);
        if (phaseShader) glDeleteProgram(phaseShader);
        // Loading screen cleanup
        if (loadingTexture) glDeleteTextures(1, &loadingTexture);
        if (loadingVAO) glDeleteVertexArrays(1, &loadingVAO);
        if (loadingVBO) glDeleteBuffers(1, &loadingVBO);
        if (loadingShader) glDeleteProgram(loadingShader);
        // On exit, we call exit for the camera controller system to make sure that the mouse is unlocked
        cameraController.exit();
        // Clear the world
        world.clear();
        // Note: we do NOT clear assets on restart (clearAllAssets) because reloading
        // all models/textures/shaders from disk is very slow. Assets are reused across
        // restarts. They will be cleared when the application exits instead.
    }
};

// Force build 2
