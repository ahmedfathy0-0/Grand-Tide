#include "boss-health-bar.hpp"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>

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

static const char* textVert = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uProjection;
uniform vec2 uOffset;
uniform vec2 uSize;
uniform vec2 uTexOffset;
uniform vec2 uTexSize;
out vec2 vTex;
void main() {
    vTex = aPos * uTexSize + uTexOffset;
    gl_Position = uProjection * vec4(aPos * uSize + uOffset, 0.0, 1.0);
}
)";

static const char* textFrag = R"(
#version 330 core
in vec2 vTex;
out vec4 fragColor;
uniform sampler2D uTex;
uniform vec4 uColor;
void main() {
    float a = texture(uTex, vTex).r;
    if (a < 0.01) discard;
    fragColor = vec4(uColor.rgb, uColor.a * a);
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
    vs = compileShader(textVert, GL_VERTEX_SHADER);
    fs = compileShader(textFrag, GL_FRAGMENT_SHADER);
    if (vs && fs) {
        textShaderProgram = linkProgram(vs, fs);
        tProjectionLoc = glGetUniformLocation(textShaderProgram, "uProjection");
        tOffsetLoc = glGetUniformLocation(textShaderProgram, "uOffset");
        tSizeLoc = glGetUniformLocation(textShaderProgram, "uSize");
        tColorLoc = glGetUniformLocation(textShaderProgram, "uColor");
        tTexLoc = glGetUniformLocation(textShaderProgram, "uTex");
        tTexOffsetLoc = glGetUniformLocation(textShaderProgram, "uTexOffset");
        tTexSizeLoc = glGetUniformLocation(textShaderProgram, "uTexSize");
        glDeleteShader(vs);
        glDeleteShader(fs);
    }
}

static bool readFileBytes(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto sz = f.tellg();
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(sz));
    f.read(reinterpret_cast<char*>(out.data()), sz);
    return f.good();
}

void BossHealthBar::bakeFont() {
    std::vector<unsigned char> ttf;
    std::vector<std::string> candidates;
    candidates.push_back(fontPath);
    candidates.push_back("C:/Windows/Fonts/arial.ttf");
    candidates.push_back("C:/Windows/Fonts/times.ttf");
    candidates.push_back("C:/Windows/Fonts/segoeui.ttf");

    bool loaded = false;
    for (const auto& p : candidates) {
        if (readFileBytes(p, ttf)) {
            loaded = true;
            break;
        }
    }
    if (!loaded) return;

    const int pw = 512, ph = 512;
    std::vector<unsigned char> bitmap(pw * ph);
    cdata = new stbtt_bakedchar[96];
    int res = stbtt_BakeFontBitmap(ttf.data(), 0, 28.0f, bitmap.data(), pw, ph, 32, 96, static_cast<stbtt_bakedchar*>(cdata));
    if (res <= 0) {
        delete[] static_cast<stbtt_bakedchar*>(cdata);
        cdata = nullptr;
        return;
    }

    glGenTextures(1, &fontTexture);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, pw, ph, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    fontReady = true;
}

void BossHealthBar::init(int screenWidth, int screenHeight, const std::string& path) {
    screenW = static_cast<float>(screenWidth);
    screenH = static_cast<float>(screenHeight);
    fontPath = path;
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
    bakeFont();
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
    if (ratio > 0.6f) targetColor = glm::vec3(1.0f, 0.78f, 0.1f);
    else if (ratio > 0.3f) targetColor = glm::vec3(1.0f, 0.45f, 0.05f);
    else targetColor = glm::vec3(0.85f, 0.08f, 0.08f);
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

void BossHealthBar::drawText(const std::string& str, float x, float y) {
    if (!fontReady || !textShaderProgram) return;
    glUseProgram(textShaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glUniform1i(tTexLoc, 0);
    glUniform4f(tColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

    stbtt_bakedchar* baked = static_cast<stbtt_bakedchar*>(cdata);
    float cx = x;
    const int pw = 512, ph = 512;
    for (char ch : str) {
        if (ch < 32 || ch >= 128) continue;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(baked, pw, ph, ch - 32, &cx, &y, &q, 1);
        float w = q.x1 - q.x0;
        float h = q.y1 - q.y0;
        if (w > 0 && h > 0) {
            // Reuse same VAO but with UV positions: we need a textured quad.
            // Our VAO has position 0..1, but text shader expects vTex = aPos.
            // However stbtt gives pixel quads. We need to map them to UV space.
            // Simpler: just render two triangles with positions in pixel space directly.
            // We'll use the bar shader's approach: scale unit quad to pixel size.
            glUniform2f(tOffsetLoc, q.x0, q.y0);
            glUniform2f(tSizeLoc, w, h);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }
}

void BossHealthBar::drawBracket(float cx, float cy, bool right) {
    const float thick = 2.0f;
    const float arm = 10.0f;
    glm::vec4 col = getHPColor((maxHP > 0.0f) ? (displayHP / maxHP) : 0.0f);
    if (right) {
        // vertical arm on right side going down
        drawQuad(cx - thick, cy, thick, arm, col);
        // horizontal arm going left
        drawQuad(cx - arm, cy, arm, thick, col);
    } else {
        // vertical arm on left side going down
        drawQuad(cx, cy, thick, arm, col);
        // horizontal arm going right
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

    // Flash mix
    if (flashTimer > 0.0f) {
        float ft = flashTimer / 0.25f;
        hpColor = glm::mix(hpColor, glm::vec4(1.0f), ft);
    }

    glm::vec4 bgColor(0.12f, 0.04f, 0.04f, 0.9f);
    glm::vec4 darkBorder(0.1f, 0.1f, 0.1f, 1.0f);
    glm::vec4 glowColor = hpColor;
    glowColor.a = 0.4f;

    // 1. Background
    drawQuad(barX, barY, barW, barH, bgColor);

    // 2. HP fill
    float fillW = barW * ratio;
    if (fillW > 0.0f) drawQuad(barX, barY, fillW, barH, hpColor);

    // 3. Outer glow (1px outside)
    drawQuad(barX - 1.0f, barY - 1.0f, barW + 2.0f, 1.0f, glowColor); // top
    drawQuad(barX - 1.0f, barY + barH, barW + 2.0f, 1.0f, glowColor); // bottom
    drawQuad(barX - 1.0f, barY - 1.0f, 1.0f, barH + 2.0f, glowColor); // left
    drawQuad(barX + barW, barY - 1.0f, 1.0f, barH + 2.0f, glowColor); // right

    // 4. Inner dark border (2px)
    drawQuad(barX, barY, barW, 2.0f, darkBorder);
    drawQuad(barX, barY + barH - 2.0f, barW, 2.0f, darkBorder);
    drawQuad(barX, barY, 2.0f, barH, darkBorder);
    drawQuad(barX + barW - 2.0f, barY, 2.0f, barH, darkBorder);

    // 5. Brackets
    drawBracket(barX - 12.0f, barY + (barH - 10.0f) * 0.5f, false);
    drawBracket(barX + barW + 12.0f, barY + (barH - 10.0f) * 0.5f, true);

    // 6. Name text
    if (fontReady && textShaderProgram) {
        glUseProgram(textShaderProgram);
        glUniformMatrix4fv(tProjectionLoc, 1, GL_FALSE, &proj[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fontTexture);
        glUniform1i(tTexLoc, 0);
        glUniform4f(tColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

        std::string name = "KRAKEN";
        stbtt_bakedchar* baked = static_cast<stbtt_bakedchar*>(cdata);
        float tw = 0.0f;
        for (char ch : name) {
            if (ch >= 32 && ch < 128) tw += baked[ch - 32].xadvance;
        }
        float tx = (screenW - tw) * 0.5f;
        float ty = barY - 6.0f - 28.0f; // 6px gap above bar, 28px font height approx
        // Actually stbtt_BakeFontBitmap height is approximate; baseline is better.
        // We'll just use a rough offset and draw.
        // Compute y baseline: barY - 6 is top of text area. baseline slightly lower.
        float textBaseY = barY - 6.0f + 22.0f; // approximate baseline

        float cx = tx;
        const int pw = 512, ph = 512;
        for (char ch : name) {
            if (ch < 32 || ch >= 128) continue;
            stbtt_aligned_quad q;
            float cy = textBaseY;
            stbtt_GetBakedQuad(baked, pw, ph, ch - 32, &cx, &cy, &q, 1);
            float w = q.x1 - q.x0;
            float h = q.y1 - q.y0;
            if (w > 0 && h > 0) {
                glUniform2f(tOffsetLoc, q.x0, q.y0);
                glUniform2f(tSizeLoc, w, h);
                glUniform2f(tTexOffsetLoc, q.s0, q.t0);
                glUniform2f(tTexSizeLoc, q.s1 - q.s0, q.t1 - q.t0);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
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
    if (textShaderProgram) { glDeleteProgram(textShaderProgram); textShaderProgram = 0; }
    if (fontTexture) { glDeleteTextures(1, &fontTexture); fontTexture = 0; }
    if (cdata) { delete[] static_cast<stbtt_bakedchar*>(cdata); cdata = nullptr; }
    fontReady = false;
}

}
