#pragma once

#include <application.hpp>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <video/video-player.hpp>
#include <iostream>

// Plays a video intro. Any key press or mouse click skips to menu.
class Introstate : public our::State {
    our::VideoPlayer* videoPlayer = nullptr;
    bool videoFinished = false;

    // Minimal fullscreen quad for drawing the video texture
    GLuint quadVAO = 0, quadVBO = 0, quadShader = 0;

    static GLuint compileShader(const char* vert, const char* frag) {
        auto compile = [](const char* src, GLenum type) -> GLuint {
            GLuint sh = glCreateShader(type);
            glShaderSource(sh, 1, &src, nullptr);
            glCompileShader(sh);
            int ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (!ok) { char buf[512]; glGetShaderInfoLog(sh, 512, nullptr, buf);
                std::cerr << "[Intro] shader error: " << buf << std::endl; glDeleteShader(sh); return 0; }
            return sh;
        };
        GLuint vs = compile(vert, GL_VERTEX_SHADER);
        GLuint fs = compile(frag, GL_FRAGMENT_SHADER);
        if (!vs || !fs) { if(vs) glDeleteShader(vs); if(fs) glDeleteShader(fs); return 0; }
        GLuint p = glCreateProgram();
        glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
        int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) { char buf[512]; glGetProgramInfoLog(p, 512, nullptr, buf);
            std::cerr << "[Intro] link error: " << buf << std::endl; glDeleteProgram(p); p = 0; }
        glDeleteShader(vs); glDeleteShader(fs);
        return p;
    }

    void onInitialize() override {
        videoPlayer = new our::VideoPlayer();
        if (!videoPlayer->load("assets/videos/output_cut.mp4")) {
            std::cerr << "[Intro] Failed to load video, skipping to menu" << std::endl;
            videoFinished = true;
        }

        // Shader: fullscreen quad with video texture
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
            uniform sampler2D uTex;
            in vec2 vUV;
            out vec4 fragColor;
            void main() {
                fragColor = vec4(texture(uTex, vUV).rgb, 1.0);
            }
        )";
        quadShader = compileShader(vert, frag);

        float quad[] = {
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
            0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    void onDraw(double deltaTime) override {
        auto& keyboard = getApp()->getKeyboard();
        auto& mouse = getApp()->getMouse();

        // Skip on any key or mouse click
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

        // Advance video
        if (videoPlayer->isLoaded()) {
            if (!videoPlayer->update(deltaTime)) {
                videoFinished = true;
            }
        }

        // Draw — set viewport to cover the whole window
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
