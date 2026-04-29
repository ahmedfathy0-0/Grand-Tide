#include "damage-flash.hpp"

#include <stb/stb_image.h>

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace our {

// --- Textured blood splatter shader ---
static const char* texVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uProjection;
uniform vec2 uOffset;
uniform vec2 uSize;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = uProjection * vec4(aPos * uSize + uOffset, 0.0, 1.0);
}
)";

static const char* texFrag = R"(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uBloodTex;
uniform float uAlpha;
void main() {
    vec4 tex = texture(uBloodTex, vUV);
    fragColor = vec4(tex.rgb, tex.a * uAlpha);
    if (fragColor.a < 0.01) discard;
}
)";

// --- Plain fallback shader (no texture) ---
static const char* plainVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uProjection;
uniform vec2 uOffset;
uniform vec2 uSize;
void main() {
    gl_Position = uProjection * vec4(aPos * uSize + uOffset, 0.0, 1.0);
}
)";

static const char* plainFrag = R"(
#version 330 core
out vec4 fragColor;
uniform float uAlpha;
uniform vec3 uColor;
void main() {
    fragColor = vec4(uColor, uAlpha);
}
)";

static GLuint compileShader(const char* src, GLenum type) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(sh, 512, nullptr, buf);
        std::cerr << "[DamageFlash] shader error:\n" << buf << std::endl;
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint linkProgram(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(p, 512, nullptr, buf);
        std::cerr << "[DamageFlash] link error:\n" << buf << std::endl;
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

void DamageFlash::buildShaders() {
    // Textured shader
    GLuint vs = compileShader(texVert, GL_VERTEX_SHADER);
    GLuint fs = compileShader(texFrag, GL_FRAGMENT_SHADER);
    if (vs && fs) {
        texShaderProgram = linkProgram(vs, fs);
        tProjLoc = glGetUniformLocation(texShaderProgram, "uProjection");
        tOffLoc  = glGetUniformLocation(texShaderProgram, "uOffset");
        tSizeLoc = glGetUniformLocation(texShaderProgram, "uSize");
        tAlphaLoc = glGetUniformLocation(texShaderProgram, "uAlpha");
        tTexLoc  = glGetUniformLocation(texShaderProgram, "uBloodTex");
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
    // Plain fallback shader
    vs = compileShader(plainVert, GL_VERTEX_SHADER);
    fs = compileShader(plainFrag, GL_FRAGMENT_SHADER);
    if (vs && fs) {
        plainShaderProgram = linkProgram(vs, fs);
        pProjLoc  = glGetUniformLocation(plainShaderProgram, "uProjection");
        pOffLoc   = glGetUniformLocation(plainShaderProgram, "uOffset");
        pSizeLoc  = glGetUniformLocation(plainShaderProgram, "uSize");
        pAlphaLoc = glGetUniformLocation(plainShaderProgram, "uAlpha");
        pColorLoc = glGetUniformLocation(plainShaderProgram, "uColor");
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
}

bool DamageFlash::loadBloodTexture(const std::string& path) {
    int w, h, ch;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) {
        std::cerr << "[DamageFlash] blood texture not found: " << path
                  << " — falling back to plain red overlay" << std::endl;
        return false;
    }
    glGenTextures(1, &bloodTexture);
    glBindTexture(GL_TEXTURE_2D, bloodTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    return true;
}

void DamageFlash::init(const std::string& texturePath) {
    // Unit quad with UV: pos(x,y), uv(u,v)
    float verts[] = {
        // pos    uv
        0,0,   0,0,
        1,0,   1,0,
        0,1,   0,1,
        0,1,   0,1,
        1,0,   1,0,
        1,1,   1,1
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glBindVertexArray(0);

    buildShaders();
    textureLoaded = loadBloodTexture(texturePath);
    flashAlpha = 0.0f;
}

void DamageFlash::triggerHit() {
    flashAlpha = 1.0f;
}

void DamageFlash::update(float deltaTime) {
    if (flashAlpha > 0.0f) {
        flashAlpha -= deltaTime / 3.0f;
        if (flashAlpha < 0.0f) flashAlpha = 0.0f;
    }
}

void DamageFlash::render(int screenWidth, int screenHeight) {
    if (flashAlpha <= 0.01f) return;

    bool depthOn = glIsEnabled(GL_DEPTH_TEST) != GL_FALSE;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float sw = static_cast<float>(screenWidth);
    float sh = static_cast<float>(screenHeight);
    glm::mat4 proj = glm::ortho(0.0f, sw, sh, 0.0f, -1.0f, 1.0f);

    glBindVertexArray(quadVAO);

    if (textureLoaded && bloodTexture && texShaderProgram) {
        // Blood splatter texture path
        glUseProgram(texShaderProgram);
        glUniformMatrix4fv(tProjLoc, 1, GL_FALSE, &proj[0][0]);
        glUniform2f(tOffLoc, 0.0f, 0.0f);
        glUniform2f(tSizeLoc, sw, sh);
        glUniform1f(tAlphaLoc, flashAlpha);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bloodTexture);
        glUniform1i(tTexLoc, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } else if (plainShaderProgram) {
        // Fallback: plain dark red fullscreen quad
        glUseProgram(plainShaderProgram);
        glUniformMatrix4fv(pProjLoc, 1, GL_FALSE, &proj[0][0]);
        glUniform2f(pOffLoc, 0.0f, 0.0f);
        glUniform2f(pSizeLoc, sw, sh);
        glUniform1f(pAlphaLoc, flashAlpha * 0.5f);
        glUniform3f(pColorLoc, 0.5f, 0.0f, 0.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (depthOn) glEnable(GL_DEPTH_TEST);
}

void DamageFlash::destroy() {
    if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
    if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
    if (texShaderProgram) { glDeleteProgram(texShaderProgram); texShaderProgram = 0; }
    if (plainShaderProgram) { glDeleteProgram(plainShaderProgram); plainShaderProgram = 0; }
    if (bloodTexture) { glDeleteTextures(1, &bloodTexture); bloodTexture = 0; }
    textureLoaded = false;
}

}
