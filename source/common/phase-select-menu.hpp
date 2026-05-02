#pragma once

#include <gl-helpers.hpp>
#include <input/keyboard.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <iostream>

namespace our {

    // Phase selection menu: 3 cards (Sharks, Marines, Kraken).
    // Handles its own GL rendering, ImGui overlay, input, and cleanup.
    class PhaseSelectMenu {
        int selectedIndex = 0; // 0=SHARKS, 1=MARINES, 2=OCTOPUS
        GLuint textures[3] = {0, 0, 0};
        GLuint vao = 0, vbo = 0;
        GLuint shader = 0;

        static constexpr const char* vertSrc = R"(
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

        static constexpr const char* fragSrc = R"(
#version 330 core
uniform sampler2D uPhaseTex;
uniform float uHighlight;
in vec2 vUV;
out vec4 fragColor;
void main() {
    vec4 col = texture(uPhaseTex, vUV);
    if (col.a < 0.02) discard;
    float brightness = mix(0.4, 1.0, uHighlight);
    fragColor = vec4(col.rgb * brightness, col.a);
}
)";

    public:
        void init() {
            selectedIndex = 0;
            stbi_set_flip_vertically_on_load(false);
            const char* paths[3] = {
                "assets/textures/sharks_phase.png",
                "assets/textures/marine_phase.png",
                "assets/textures/kraken_phase.png"
            };
            for (int i = 0; i < 3; i++)
                textures[i] = loadTexture(paths[i]);
            shader = compileShaderProgram(vertSrc, fragSrc);
            vao = createQuadVAO(vbo);
        }

        // Returns: 0=no action, 1=phase selected, -1=back(ESC)
        int handleInput(our::Keyboard& keyboard) {
            if (keyboard.justPressed(GLFW_KEY_LEFT))
                selectedIndex = (selectedIndex - 1 + 3) % 3;
            if (keyboard.justPressed(GLFW_KEY_RIGHT))
                selectedIndex = (selectedIndex + 1) % 3;
            if (keyboard.justPressed(GLFW_KEY_ENTER))
                return 1; // Phase selected
            if (keyboard.justPressed(GLFW_KEY_ESCAPE))
                return -1; // Back
            return 0;
        }

        int getSelectedIndex() const { return selectedIndex; }

        void render() {
            if (!shader || !vao) return;
            glClearColor(0.05f, 0.05f, 0.1f, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(shader);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            float cardW = 0.38f, cardH = 0.38f, gap = 0.0005f;
            float totalW = 3.0f * cardW + 2.0f * gap;
            float startX = 0.5f - totalW / 2.0f;
            float startY = 0.5f - cardH / 2.0f;

            GLint offsetLoc = glGetUniformLocation(shader, "uOffset");
            GLint scaleLoc = glGetUniformLocation(shader, "uScale");
            GLint texLoc = glGetUniformLocation(shader, "uPhaseTex");
            GLint highlightLoc = glGetUniformLocation(shader, "uHighlight");
            glUniform1i(texLoc, 0);

            for (int i = 0; i < 3; i++) {
                float x = startX + i * (cardW + gap);
                float highlight = (i == selectedIndex) ? 1.0f : 0.0f;
                glUniform2f(offsetLoc, x, startY);
                glUniform2f(scaleLoc, cardW, cardH);
                glUniform1f(highlightLoc, highlight);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textures[i]);
                glBindVertexArray(vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            glBindVertexArray(0);
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glUseProgram(0);
        }

        void renderImGuiOverlay(float screenWidth, float screenHeight) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

            // Title
            ImGui::SetNextWindowPos(ImVec2(screenWidth * 0.5f, screenHeight * 0.08f), 0, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("PhaseSelectTitle", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("SELECT STARTING PHASE");
            ImGui::End();

            // Card positions
            float cardW_px = screenWidth * 0.20f;
            float gap_px = screenWidth * 0.05f;
            float totalW_px = 3.0f * cardW_px + 2.0f * gap_px;
            float startX_px = screenWidth * 0.5f - totalW_px / 2.0f;
            float selCenterX = startX_px + selectedIndex * (cardW_px + gap_px) + cardW_px * 0.5f;
            float cardTopY = screenHeight * 0.5f - screenHeight * 0.25f;

            // Left arrow
            ImGui::SetNextWindowPos(ImVec2(selCenterX - cardW_px * 0.6f, cardTopY + screenHeight * 0.25f * 0.5f - 15), 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("ArrowLeft", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("<<");
            ImGui::End();

            // Right arrow
            ImGui::SetNextWindowPos(ImVec2(selCenterX + cardW_px * 0.6f, cardTopY + screenHeight * 0.25f * 0.5f - 15), 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("ArrowRight", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text(">>");
            ImGui::End();

            // Phase name
            const char* phaseNames[3] = {"SHARKS", "MARINES", "KRAKEN"};
            ImGui::SetNextWindowPos(ImVec2(selCenterX, cardTopY + screenHeight * 0.25f + 20), 0, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("PhaseName", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("%s", phaseNames[selectedIndex]);
            ImGui::End();

            // Instructions
            ImGui::SetNextWindowPos(ImVec2(screenWidth * 0.5f, screenHeight * 0.92f), 0, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("PhaseSelectHelp", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("LEFT/RIGHT to select  |  ENTER to start  |  ESC to go back");
            ImGui::End();

            ImGui::PopStyleColor();
        }

        void destroy() {
            for (int i = 0; i < 3; i++) {
                if (textures[i]) glDeleteTextures(1, &textures[i]);
            }
            if (vao) glDeleteVertexArrays(1, &vao);
            if (vbo) glDeleteBuffers(1, &vbo);
            if (shader) glDeleteProgram(shader);
        }
    };

} // namespace our
