#pragma once

#include <application.hpp>
#include <gl-helpers.hpp>
#include <video/video-player.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

// Plays a video intro. Any key press or mouse click skips to menu.
class Introstate : public our::State {
    our::VideoPlayer* videoPlayer = nullptr;
    bool videoFinished = false;
    GLuint quadVAO = 0, quadVBO = 0, quadShader = 0;

    static constexpr const char* fragSrc = R"(
#version 330 core
uniform sampler2D uTex;
in vec2 vUV;
out vec4 fragColor;
void main() {
    fragColor = vec4(texture(uTex, vUV).rgb, 1.0);
}
)";

    void onInitialize() override {
        videoPlayer = new our::VideoPlayer();
        if (!videoPlayer->load("assets/videos/output_cut.mp4")) {
            std::cerr << "[Intro] Failed to load video, skipping to menu" << std::endl;
            videoFinished = true;
        }
        quadShader = our::compileShaderProgram(our::quadVertSrc, fragSrc);
        quadVAO = our::createQuadVAO(quadVBO);
    }

    void onDraw(double deltaTime) override {
        auto& keyboard = getApp()->getKeyboard();
        auto& mouse = getApp()->getMouse();

        bool skip = false;
        for (int k = 32; k <= GLFW_KEY_LAST; k++) {
            if (keyboard.justPressed(k)) { skip = true; break; }
        }
        if (mouse.justPressed(0) || mouse.justPressed(1) || mouse.justPressed(2)) skip = true;

        if (skip || videoFinished) {
            videoPlayer->stopAudio();
            getApp()->changeState("menu");
            return;
        }

        if (videoPlayer->isLoaded()) {
            if (!videoPlayer->update(deltaTime)) {
                videoFinished = true;
            }
        }

        glm::ivec2 size = getApp()->getFrameBufferSize();
        glViewport(0, 0, size.x, size.y);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (quadShader && quadVAO && videoPlayer->getTexture()) {
            glUseProgram(quadShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, videoPlayer->getTexture());
            glUniform1i(glGetUniformLocation(quadShader, "uTex"), 0);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_BLEND);

            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            glEnable(GL_DEPTH_TEST);
            glUseProgram(0);
        }
    }

    void onDestroy() override {
        if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
        if (quadVBO) glDeleteBuffers(1, &quadVBO);
        if (quadShader) glDeleteProgram(quadShader);
        delete videoPlayer;
    }
};
