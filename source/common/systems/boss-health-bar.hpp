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

    GLuint textShaderProgram = 0;
    GLint tProjectionLoc = -1;
    GLint tOffsetLoc = -1;
    GLint tSizeLoc = -1;
    GLint tColorLoc = -1;
    GLint tTexLoc = -1;
    GLint tTexOffsetLoc = -1;
    GLint tTexSizeLoc = -1;

    GLuint fontTexture = 0;
    void* cdata = nullptr;  // stbtt_bakedchar*
    bool fontReady = false;
    std::string fontPath;

    float screenW = 0.0f;
    float screenH = 0.0f;

    float targetHP = 0.0f;
    float displayHP = 0.0f;
    float maxHP = 100.0f;
    float flashTimer = 0.0f;

    glm::vec3 currentHPColor = glm::vec3(1.0f, 0.78f, 0.1f);

    void buildShaders();
    void bakeFont();
    void drawQuad(float x, float y, float w, float h, const glm::vec4& col);
    void drawText(const std::string& str, float x, float y);
    glm::vec4 getHPColor(float ratio);
    void drawBracket(float cx, float cy, bool right);

public:
    void init(int screenWidth, int screenHeight, const std::string& fontPath = "assets/fonts/pirate.ttf");
    void update(float currentHP, float maxHP, float deltaTime);
    void render();
    void resize(int newWidth, int newHeight);
    void destroy();
    void onDamageReceived();
};

}
