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

    GLuint labelShaderProgram = 0;
    GLint lProjectionLoc = -1;
    GLint lOffsetLoc = -1;
    GLint lSizeLoc = -1;
    GLint lTexLoc = -1;

    GLuint labelTexture = 0;
    int labelW = 0;
    int labelH = 0;

    float screenW = 0.0f;
    float screenH = 0.0f;

    float targetHP = 0.0f;
    float displayHP = 0.0f;
    float maxHP = 100.0f;
    float flashTimer = 0.0f;
    glm::vec3 currentHPColor = glm::vec3(1.0f, 0.78f, 0.1f);

    void buildShaders();
    bool loadLabelTexture(const std::string& path);
    void drawQuad(float x, float y, float w, float h, const glm::vec4& col);
    void drawBracket(float cx, float cy, bool right);
    glm::vec4 getHPColor(float ratio);

public:
    void init(int screenWidth, int screenHeight, const std::string& labelPath = "octopus/kraken-label.png");
    void update(float currentHP, float maxHP, float deltaTime);
    void render();
    void resize(int newWidth, int newHeight);
    void destroy();
    void onDamageReceived();
};

}
