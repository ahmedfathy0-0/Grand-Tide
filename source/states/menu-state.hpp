#pragma once

#include <application.hpp>
#include <gl-helpers.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

// Start menu: arrow keys to select Play/Exit, Enter to confirm.
class Menustate : public our::State {
    int selectedItem = 0; // 0=PLAY, 1=EXIT
    GLuint menuTextures[2] = {0, 0};
    GLuint menuVAO = 0, menuVBO = 0;
    GLuint menuShader = 0;

    static constexpr const char* fragSrc = R"(
#version 330 core
uniform sampler2D uMenuTex;
in vec2 vUV;
out vec4 fragColor;
void main() {
    vec4 col = texture(uMenuTex, vUV);
    fragColor = vec4(col.rgb, 1.0);
}
)";

    void onInitialize() override {
        selectedItem = 0;
        stbi_set_flip_vertically_on_load(false);
        menuTextures[0] = our::loadTexture("assets/textures/game_play.png");
        menuTextures[1] = our::loadTexture("assets/textures/game_exit.png");
        menuShader = our::compileShaderProgram(our::quadVertSrc, fragSrc);
        menuVAO = our::createQuadVAO(menuVBO);
    }

    void onDraw(double deltaTime) override {
        auto& keyboard = getApp()->getKeyboard();

        if (keyboard.justPressed(GLFW_KEY_UP))
            selectedItem = (selectedItem - 1 + 2) % 2;
        if (keyboard.justPressed(GLFW_KEY_DOWN))
            selectedItem = (selectedItem + 1) % 2;
        if (keyboard.justPressed(GLFW_KEY_ENTER)) {
            if (selectedItem == 0) { getApp()->changeState("play"); return; }
            else { getApp()->close(); return; }
        }

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (menuShader && menuVAO && menuTextures[selectedItem]) {
            glUseProgram(menuShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, menuTextures[selectedItem]);
            glUniform1i(glGetUniformLocation(menuShader, "uMenuTex"), 0);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);

            glBindVertexArray(menuVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
            glUseProgram(0);
        }
    }

    void onDestroy() override {
        for (int i = 0; i < 2; i++) {
            if (menuTextures[i]) glDeleteTextures(1, &menuTextures[i]);
        }
        if (menuVAO) glDeleteVertexArrays(1, &menuVAO);
        if (menuVBO) glDeleteBuffers(1, &menuVBO);
        if (menuShader) glDeleteProgram(menuShader);
    }
};