#pragma once

#include <gl-helpers.hpp>
#include <input/keyboard.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

namespace our {

    // Start menu overlay: 2 items (Play, Exit).
    // Handles its own input, rendering, and cleanup.
    class StartMenu {
        int selectedItem = 0; // 0=PLAY, 1=EXIT
        GLuint textures[2] = {0, 0};
        GLuint vao = 0, vbo = 0;
        GLuint shader = 0;

        static constexpr const char* fragSrc = R"(
#version 330 core
uniform sampler2D uStartTex;
in vec2 vUV;
out vec4 fragColor;
void main() {
    vec4 col = texture(uStartTex, vUV);
    fragColor = vec4(col.rgb, 1.0);
}
)";

    public:
        void init() {
            selectedItem = 0;
            stbi_set_flip_vertically_on_load(false);
            textures[0] = loadTexture("assets/textures/game_play.png");
            textures[1] = loadTexture("assets/textures/game_exit.png");
            shader = compileShaderProgram(quadVertSrc, fragSrc);
            vao = createQuadVAO(vbo);
        }

        // Returns: 0=no action, 1=play, 2=exit
        int handleInput(our::Keyboard& keyboard) {
            if (keyboard.justPressed(GLFW_KEY_UP))
                selectedItem = (selectedItem - 1 + 2) % 2;
            if (keyboard.justPressed(GLFW_KEY_DOWN))
                selectedItem = (selectedItem + 1) % 2;
            if (keyboard.justPressed(GLFW_KEY_ENTER)) {
                if (selectedItem == 0) return 1; // Play
                if (selectedItem == 1) return 2; // Exit
            }
            return 0;
        }

        void render() {
            if (!shader || !vao || !textures[selectedItem]) return;
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(shader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures[selectedItem]);
            glUniform1i(glGetUniformLocation(shader, "uStartTex"), 0);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
            glUseProgram(0);
        }

        void destroy() {
            for (int i = 0; i < 2; i++) {
                if (textures[i]) glDeleteTextures(1, &textures[i]);
            }
            if (vao) glDeleteVertexArrays(1, &vao);
            if (vbo) glDeleteBuffers(1, &vbo);
            if (shader) glDeleteProgram(shader);
        }
    };

} // namespace our
