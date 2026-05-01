#pragma once

#include <application.hpp>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <iostream>

// Start menu: arrow keys to select Play/Exit, Enter to confirm.
// Comes after the video intro.
class Menustate : public our::State {
    int selectedItem = 0; // 0=PLAY, 1=EXIT
    GLuint menuTextures[2] = {0, 0};
    GLuint menuVAO = 0, menuVBO = 0;
    GLuint menuShader = 0;

    static GLuint compileShader(const char* vert, const char* frag) {
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

    void onInitialize() override {
        selectedItem = 0;

        // Load 2 menu textures
        const char* paths[2] = {
            "assets/textures/game_play.png",
            "assets/textures/game_exit.png"
        };
        for (int i = 0; i < 2; i++) {
            int w, h, ch;
            unsigned char* data = stbi_load(paths[i], &w, &h, &ch, 4);
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
                std::cerr << "[Menu] Failed to load: " << paths[i] << std::endl;
            }
        }

        // Compile menu shader
        static const char* vert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = vec2(aPos.x, 1.0 - aPos.y);
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
}
)";
        static const char* frag = R"(
#version 330 core
uniform sampler2D uMenuTex;
in vec2 vUV;
out vec4 fragColor;
void main() {
    vec4 col = texture(uMenuTex, vUV);
    fragColor = vec4(col.rgb, 1.0);
}
)";
        menuShader = compileShader(vert, frag);

        // Create fullscreen quad VAO/VBO
        float quad[12] = {
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
            0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f
        };
        glGenVertexArrays(1, &menuVAO);
        glGenBuffers(1, &menuVBO);
        glBindVertexArray(menuVAO);
        glBindBuffer(GL_ARRAY_BUFFER, menuVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    void onDraw(double deltaTime) override {
        auto& keyboard = getApp()->getKeyboard();

        if (keyboard.justPressed(GLFW_KEY_UP)) {
            selectedItem = (selectedItem - 1 + 2) % 2;
        }
        if (keyboard.justPressed(GLFW_KEY_DOWN)) {
            selectedItem = (selectedItem + 1) % 2;
        }
        if (keyboard.justPressed(GLFW_KEY_ENTER)) {
            if (selectedItem == 0) {
                getApp()->changeState("play");
                return;
            } else {
                getApp()->close();
                return;
            }
        }

        // Clear screen
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw menu fullscreen
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