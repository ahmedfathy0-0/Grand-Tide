#pragma once

#include <glad/gl.h>
#include <string>

namespace our {

class DamageFlash {
private:
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;
    GLuint texShaderProgram = 0;
    GLuint plainShaderProgram = 0;

    GLint tProjLoc = -1, tOffLoc = -1, tSizeLoc = -1, tAlphaLoc = -1, tTexLoc = -1;
    GLint pProjLoc = -1, pOffLoc = -1, pSizeLoc = -1, pAlphaLoc = -1, pColorLoc = -1;

    GLuint bloodTexture = 0;
    bool textureLoaded = false;

    float flashAlpha = 0.0f;

    void buildShaders();
    bool loadBloodTexture(const std::string& path);

public:
    void init(const std::string& texturePath = "assets/textures/blood-splatter.png");
    void triggerHit();
    void update(float deltaTime);
    void render(int screenWidth, int screenHeight);
    void destroy();
};

}
