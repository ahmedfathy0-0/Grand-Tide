#pragma once

#include <gl-helpers.hpp>
#include <input/keyboard.hpp>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <iostream>

namespace our {

    // Pause menu overlay: 3 items (Continue, Restart, Exit).
    // Handles its own input, rendering, and cleanup.
    class PauseMenu {
        bool isPaused = false;
        int selectedItem = 0; // 0=CONTINUE, 1=RESTART, 2=EXIT
        GLuint textures[3] = {0, 0, 0};
        GLuint vao = 0, vbo = 0;
        GLuint shader = 0;

        static constexpr const char* fragSrc = R"(
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

    public:
        void init() {
            isPaused = false;
            selectedItem = 0;
            stbi_set_flip_vertically_on_load(false);
            textures[0] = loadTexture("assets/textures/menu_continue.png");
            textures[1] = loadTexture("assets/textures/menu_restart.png");
            textures[2] = loadTexture("assets/textures/menu_exit.png");
            shader = compileShaderProgram(quadVertSrc, fragSrc);
            vao = createQuadVAO(vbo);
        }

        // Returns:
        //   0 = no action (game continues normally)
        //   1 = unpause (resume game)
        //   2 = restart (change to "play" state)
        //   3 = exit (close app)
        int handleInput(our::Keyboard& keyboard) {
            // Note: ESC toggle is handled externally via togglePause()
            // to bypass ImGui keyboard capture issues

            if (!isPaused) return 0;

            if (keyboard.justPressed(GLFW_KEY_UP))
                selectedItem = (selectedItem - 1 + 3) % 3;
            if (keyboard.justPressed(GLFW_KEY_DOWN))
                selectedItem = (selectedItem + 1) % 3;
            if (keyboard.justPressed(GLFW_KEY_ENTER)) {
                if (selectedItem == 0) { isPaused = false; return 1; }
                else if (selectedItem == 1) { isPaused = false; return 2; }
                else if (selectedItem == 2) { isPaused = false; return 3; }
            }

            return 0; // still paused, no action
        }

        // Render the pause menu overlay (call AFTER rendering the world)
        void renderOverlay() {
            if (!isPaused) return;
            if (shader && vao && textures[selectedItem]) {
                glUseProgram(shader);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, textures[selectedItem]);
                glUniform1i(glGetUniformLocation(shader, "uMenuTex"), 0);
                glUniform1f(glGetUniformLocation(shader, "uAlpha"), 1.0f);

                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);

                glBindVertexArray(vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                glEnable(GL_DEPTH_TEST);
                glDisable(GL_BLEND);
                glUseProgram(0);
            }
        }

        // Legacy: handle input and render together
        int handleInputAndRender(our::Keyboard& keyboard) {
            int action = handleInput(keyboard);
            renderOverlay();
            return action;
        }

        bool paused() const { return isPaused; }
        void togglePause() {
            isPaused = !isPaused;
            if (isPaused) selectedItem = 0;
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
