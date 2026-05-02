#pragma once

#include <gl-helpers.hpp>
#include <iostream>

namespace our {

    // Loading screen overlay: 3 images (start, mid, end) shown during staged loading.
    // Handles its own GL resources and rendering. The loading *logic* (what to load
    // at each step) is driven by the caller via setProgress().
    class LoadingScreen {
        GLuint textures[3] = {0, 0, 0}; // [0]=start, [1]=mid, [2]=end
        GLuint vao = 0, vbo = 0;
        GLuint shader = 0;

        static constexpr const char* fragSrc = R"(
#version 330 core
uniform sampler2D uLoadingTex;
in vec2 vUV;
out vec4 fragColor;
void main() {
    fragColor = texture(uLoadingTex, vUV);
}
)";

    public:
        void init() {
            stbi_set_flip_vertically_on_load(false);
            const char* paths[3] = {
                "assets/textures/loading_start.png",
                "assets/textures/loading.png",
                "assets/textures/loading_end.png"
            };
            for (int i = 0; i < 3; i++)
                textures[i] = loadTexture(paths[i]);
            shader = compileShaderProgram(quadVertSrc, fragSrc);
            vao = createQuadVAO(vbo);
        }

        // Render the loading screen with the given progress [0..1]
        void render(float progress) {
            int texIdx = (progress < 0.25f) ? 0 : (progress < 0.75f) ? 1 : 2;
            GLuint activeTex = textures[texIdx];
            if (!shader || !vao || !activeTex) return;

            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(shader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, activeTex);
            glUniform1i(glGetUniformLocation(shader, "uLoadingTex"), 0);
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
            for (int i = 0; i < 3; i++) {
                if (textures[i]) glDeleteTextures(1, &textures[i]);
            }
            if (vao) glDeleteVertexArrays(1, &vao);
            if (vbo) glDeleteBuffers(1, &vbo);
            if (shader) glDeleteProgram(shader);
        }
    };

} // namespace our
