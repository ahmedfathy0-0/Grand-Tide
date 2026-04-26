#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>

namespace our {

    class BossHealthBar {
    private:
        GLuint quadVAO = 0;
        GLuint quadVBO = 0;
        GLuint shaderProgram = 0;
        GLint uProjectionLoc = -1;
        GLint uOffsetLoc = -1;
        GLint uSizeLoc = -1;
        GLint uColorLoc = -1;

        // Label texture (pre-baked "KRAKEN" text)
        GLuint labelTexture = 0;
        GLint labelTexW = 0;
        GLint labelTexH = 0;
        GLuint labelShaderProgram = 0;
        GLint lProjectionLoc = -1;
        GLint lOffsetLoc = -1;
        GLint lSizeLoc = -1;
        GLint lTexLoc = -1;

        float screenW = 0.0f;
        float screenH = 0.0f;

        float targetHP = 0.0f;
        float displayHP = 0.0f;
        float maxHP = 100.0f;

    public:
        void init(int screenWidth, int screenHeight);
        void update(float currentHP, float maxHP, float deltaTime);
        void render();
        void resize(int newWidth, int newHeight);
        void destroy();

        bool loadLabelTexture(const std::string& path);
    };

}
