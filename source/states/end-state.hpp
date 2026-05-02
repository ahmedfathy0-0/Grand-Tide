#pragma once

#include <application.hpp>
#include <systems/ocean-audio.hpp>
#include <gl-helpers.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstdlib>
#include <cmath>

class Endstate : public our::State {
    our::OceanAudio endingAudio;
    GLuint endingTexture = 0;

    // DVD-screensaver bouncing state
    float posX = 100.0f, posY = 100.0f;
    float velX = 150.0f, velY = 120.0f; // pixels per second
    float cardW = 0.0f, cardH = 0.0f;   // measured each frame

    // Color state (text tint)
    float colR = 1.0f, colG = 0.84f, colB = 0.0f; // gold initially

    void randomColor() {
        colR = 0.3f + 0.7f * ((float)std::rand() / RAND_MAX);
        colG = 0.3f + 0.7f * ((float)std::rand() / RAND_MAX);
        colB = 0.3f + 0.7f * ((float)std::rand() / RAND_MAX);
    }

    void onInitialize() override {
        endingTexture = our::loadTexture("assets/textures/ending.png");
        endingAudio.start("assets/audios/ending.mp3");
        endingAudio.setVolume(0.4f);
        std::srand((unsigned)glfwGetTime());
    }

    void onDraw(double deltaTime) override {
        float dt = (float)deltaTime;

        // Move the card
        posX += velX * dt;
        posY += velY * dt;

        // Bounce off edges
        auto size = getApp()->getFrameBufferSize();
        bool hit = false;
        if (posX <= 0.0f) { posX = 0.0f; velX = std::abs(velX); hit = true; }
        if (posY <= 0.0f) { posY = 0.0f; velY = std::abs(velY); hit = true; }
        if (cardW > 0.0f && posX + cardW >= size.x) { posX = size.x - cardW; velX = -std::abs(velX); hit = true; }
        if (cardH > 0.0f && posY + cardH >= size.y) { posY = size.y - cardH; velY = -std::abs(velY); hit = true; }
        if (hit) randomColor();

        // ESC returns to menu
        if (glfwGetKey(getApp()->getWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            getApp()->changeState("menu");
        }
    }

    void onImmediateGui() override {
        auto size = getApp()->getFrameBufferSize();

        // Draw ending background image fullscreen
        if (endingTexture != 0) {
            ImGui::GetBackgroundDrawList()->AddImage(
                (ImTextureID)(intptr_t)endingTexture,
                ImVec2(0, 0), ImVec2(size.x, size.y)
            );
        }

        // Draw the bouncing credits card
        ImGui::SetNextWindowPos(ImVec2(posX, posY));
        ImGui::SetNextWindowBgAlpha(0.85f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 215));
        ImGui::Begin("Credits", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::SetWindowFontScale(2.5f);
        ImGui::TextColored(ImVec4(colR, colG, colB, 1.0f), "VICTORY!");
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Game Developers:");
        ImGui::SetWindowFontScale(1.2f);
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "  A.Gamal");
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "  A.Fathy");
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "  M.Yasser");
        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "  Y.Adel");
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Press ESC to return to menu");

        // Measure card size after drawing content
        ImVec2 curSize = ImGui::GetWindowSize();
        if (curSize.x > 0.0f && curSize.y > 0.0f) {
            cardW = curSize.x;
            cardH = curSize.y;
        }

        ImGui::End();
        ImGui::PopStyleColor();
    }

    void onDestroy() override {
        endingAudio.stop();
        if (endingTexture) {
            glDeleteTextures(1, &endingTexture);
            endingTexture = 0;
        }
    }
};
