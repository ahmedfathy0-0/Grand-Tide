#include "boss-health-bar.hpp"

#include <stb/stb_image.h>

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

namespace our {

static const char* barVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uProjection;
uniform vec2 uOffset;
uniform vec2 uSize;
void main() {
    gl_Position = uProjection * vec4(aPos * uSize + uOffset, 0.0, 1.0);
}
)";

static const char* barFrag = R"(
#version 330 core
out vec4 fragColor;
uniform vec4 uColor;
void main() {
    fragColor = uColor;
}
)";

static const char* labelVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uProjection;
uniform vec2 uOffset;
uniform vec2 uSize;
out vec2 vTex;
void main() {
    vTex = aPos;
    gl_Position = uProjection * vec4(aPos * uSize + uOffset, 0.0, 1.0);
}
)";

static const char* labelFrag = R"(
#version 330 core
in vec2 vTex;
out vec4 fragColor;
uniform sampler2D uTex;
void main() {
    fragColor = texture(uTex, vTex);
    if (fragColor.a < 0.01) discard;
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
        std::cerr << "[BossHealthBar] shader error:\n" << buf << std::endl;
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
        std::cerr << "[BossHealthBar] link error:\n" << buf << std::endl;
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

void BossHealthBar::buildShaders() {
    GLuint vs = compileShader(barVert, GL_VERTEX_SHADER);
    GLuint fs = compileShader(barFrag, GL_FRAGMENT_SHADER);
    if (vs && fs) {
        shaderProgram = linkProgram(vs, fs);
        uProjectionLoc = glGetUniformLocation(shaderProgram, "uProjection");
        uOffsetLoc = glGetUniformLocation(shaderProgram, "uOffset");
        uSizeLoc = glGetUniformLocation(shaderProgram, "uSize");
        uColorLoc = glGetUniformLocation(shaderProgram, "uColor");
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
    vs = compileShader(labelVert, GL_VERTEX_SHADER);
    fs = compileShader(labelFrag, GL_FRAGMENT_SHADER);
    if (vs && fs) {
        labelShaderProgram = linkProgram(vs, fs);
        lProjectionLoc = glGetUniformLocation(labelShaderProgram, "uProjection");
        lOffsetLoc = glGetUniformLocation(labelShaderProgram, "uOffset");
        lSizeLoc = glGetUniformLocation(labelShaderProgram, "uSize");
        lTexLoc = glGetUniformLocation(labelShaderProgram, "uTex");
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
}

bool BossHealthBar::loadLabelTexture(const std::string& path) {
    int w, h, ch;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) {
        std::cerr << "[BossHealthBar] label texture not found: " << path << std::endl;
        return false;
    }
    glGenTextures(1, &labelTexture);
    glBindTexture(GL_TEXTURE_2D, labelTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    labelW = w;
    labelH = h;
    return true;
}

void BossHealthBar::init(int screenWidth, int screenHeight, const std::string& labelPath) {
    screenW = static_cast<float>(screenWidth);
    screenH = static_cast<float>(screenHeight);
    currentHPColor = glm::vec3(1.0f, 0.78f, 0.1f);

    float verts[] = {
        0,0, 1,0, 0,1,
        0,1, 1,0, 1,1
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glBindVertexArray(0);

    buildShaders();
    loadLabelTexture(labelPath);
}

void BossHealthBar::update(float currentHP, float maxHPVal, float deltaTime) {
    targetHP = currentHP;
    maxHP = maxHPVal;

    float diff = targetHP - displayHP;
    if (std::abs(diff) < 0.01f) {
        displayHP = targetHP;
    } else {
        displayHP += diff * 4.0f * deltaTime;
        if ((diff > 0 && displayHP > targetHP) || (diff < 0 && displayHP < targetHP))
            displayHP = targetHP;
    }
    if (maxHP > 0 && displayHP > maxHP) displayHP = maxHP;
    if (displayHP < 0) displayHP = 0;

    if (flashTimer > 0.0f) {
        flashTimer -= deltaTime;
        if (flashTimer < 0.0f) flashTimer = 0.0f;
    }

    float ratio = (maxHP > 0.0f) ? (displayHP / maxHP) : 0.0f;
    glm::vec3 targetColor;
    if (!isRevived) {
        if (ratio > 0.6f)      targetColor = glm::vec3(1.0f, 0.78f, 0.1f);   // gold
        else if (ratio > 0.3f) targetColor = glm::vec3(1.0f, 0.45f, 0.05f);  // orange
        else                   targetColor = glm::vec3(0.85f, 0.08f, 0.08f); // red
    } else {
        if (ratio > 0.6f)      targetColor = glm::vec3(0.6f, 0.1f, 0.8f);   // bright purple
        else if (ratio > 0.3f) targetColor = glm::vec3(0.4f, 0.05f, 0.6f);  // dark purple
        else                   targetColor = glm::vec3(0.25f, 0.0f, 0.4f);  // near black purple
    }
    float ct = deltaTime / 0.3f;
    if (ct > 1.0f) ct = 1.0f;
    currentHPColor = glm::mix(currentHPColor, targetColor, ct);
}

glm::vec4 BossHealthBar::getHPColor(float ratio) {
    (void)ratio;
    return glm::vec4(currentHPColor, 1.0f);
}

void BossHealthBar::drawQuad(float x, float y, float w, float h, const glm::vec4& col) {
    glUseProgram(shaderProgram);
    glUniform4f(uColorLoc, col.r, col.g, col.b, col.a);
    glUniform2f(uOffsetLoc, x, y);
    glUniform2f(uSizeLoc, w, h);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void BossHealthBar::drawBracket(float cx, float cy, bool right) {
    const float thick = 2.0f;
    const float arm = 10.0f;
    glm::vec4 col = getHPColor((maxHP > 0.0f) ? (displayHP / maxHP) : 0.0f);
    if (right) {
        drawQuad(cx - thick, cy, thick, arm, col);
        drawQuad(cx - arm, cy, arm, thick, col);
    } else {
        drawQuad(cx, cy, thick, arm, col);
        drawQuad(cx, cy, arm, thick, col);
    }
}

void BossHealthBar::render() {
    if (!shaderProgram || !quadVAO) return;
    bool depthOn = glIsEnabled(GL_DEPTH_TEST) != GL_FALSE;
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(quadVAO);

    glm::mat4 proj = glm::ortho(0.0f, screenW, screenH, 0.0f, -1.0f, 1.0f);
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, &proj[0][0]);

    float barW = screenW * 0.55f;
    float barH = 16.0f;
    float barX = (screenW - barW) * 0.5f;
    float barY = 24.0f;

    float ratio = (maxHP > 0.0f) ? (displayHP / maxHP) : 0.0f;
    glm::vec4 hpColor = getHPColor(ratio);

    if (flashTimer > 0.0f) {
        float ft = flashTimer / 0.25f;
        hpColor = glm::mix(hpColor, glm::vec4(1.0f), ft);
    }

    glm::vec4 bgColor(0.12f, 0.04f, 0.04f, 0.9f);
    glm::vec4 darkBorder(0.1f, 0.1f, 0.1f, 1.0f);
    glm::vec4 glowColor = hpColor;
    if (isRevived) {
        float pulse = 0.4f + 0.3f * std::sin((float)glfwGetTime() * 4.0f);
        glowColor.a = pulse;
    } else {
        glowColor.a = 0.4f;
    }

    // 1. Background
    drawQuad(barX, barY, barW, barH, bgColor);

    // 2. HP fill
    float fillW = barW * ratio;
    if (fillW > 0.0f) drawQuad(barX, barY, fillW, barH, hpColor);

    // 3. Outer glow (1px outside)
    drawQuad(barX - 1.0f, barY - 1.0f, barW + 2.0f, 1.0f, glowColor);
    drawQuad(barX - 1.0f, barY + barH, barW + 2.0f, 1.0f, glowColor);
    drawQuad(barX - 1.0f, barY - 1.0f, 1.0f, barH + 2.0f, glowColor);
    drawQuad(barX + barW, barY - 1.0f, 1.0f, barH + 2.0f, glowColor);

    // 4. Inner dark border (2px)
    drawQuad(barX, barY, barW, 2.0f, darkBorder);
    drawQuad(barX, barY + barH - 2.0f, barW, 2.0f, darkBorder);
    drawQuad(barX, barY, 2.0f, barH, darkBorder);
    drawQuad(barX + barW - 2.0f, barY, 2.0f, barH, darkBorder);

    // 5. Brackets
    drawBracket(barX - 12.0f, barY + (barH - 10.0f) * 0.5f, false);
    drawBracket(barX + barW + 12.0f, barY + (barH - 10.0f) * 0.5f, true);

    // 6. Label texture
if (labelTexture && labelShaderProgram) {
        glUseProgram(labelShaderProgram);
        glUniformMatrix4fv(lProjectionLoc, 1, GL_FALSE, &proj[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, labelTexture);
        glUniform1i(lTexLoc, 0);

        float displayLW = barW * 0.30f;
        float displayLH = displayLW * ((float)labelH / (float)labelW);
        float lx = (screenW - displayLW) * 0.5f;
        float ly = barY + barH ;

        glUniform2f(lOffsetLoc, lx, ly);
        glUniform2f(lSizeLoc, displayLW, displayLH);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (depthOn) glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void BossHealthBar::resize(int newWidth, int newHeight) {
    screenW = static_cast<float>(newWidth);
    screenH = static_cast<float>(newHeight);
}

void BossHealthBar::onDamageReceived() {
    flashTimer = 0.25f;
}

void BossHealthBar::destroy() {
    if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
    if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
    if (shaderProgram) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
    if (labelShaderProgram) { glDeleteProgram(labelShaderProgram); labelShaderProgram = 0; }
    if (labelTexture) { glDeleteTextures(1, &labelTexture); labelTexture = 0; }
}

}
