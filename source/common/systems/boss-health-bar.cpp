#include "boss-health-bar.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

// Optional: to load the label texture from file. STB_IMAGE_IMPLEMENTATION
// is already defined in texture-utils.cpp so we only include the header here.
#include <stb/stb_image.h>

namespace our {

    static const char* bossBarVert = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        uniform mat4 uProjection;
        uniform vec2 uOffset;
        uniform vec2 uSize;
        void main() {
            gl_Position = uProjection * vec4(aPos * uSize + uOffset, 0.0, 1.0);
        }
    )";

    static const char* bossBarFrag = R"(
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
        out vec2 vTexCoord;
        void main() {
            vTexCoord = aPos;
            gl_Position = uProjection * vec4(aPos * uSize + uOffset, 0.0, 1.0);
        }
    )";

    static const char* labelFrag = R"(
        #version 330 core
        in vec2 vTexCoord;
        out vec4 fragColor;
        uniform sampler2D uTexture;
        void main() {
            vec4 tex = texture(uTexture, vTexCoord);
            if(tex.a < 0.1) discard;
            fragColor = tex;
        }
    )";

    static GLuint compileShader(const char* source, GLenum type) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "[BossHealthBar] Shader compilation failed:\n" << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    static GLuint linkProgram(GLuint vert, GLuint frag) {
        GLuint program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);
        glLinkProgram(program);
        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "[BossHealthBar] Program linking failed:\n" << infoLog << std::endl;
            glDeleteProgram(program);
            return 0;
        }
        return program;
    }

    static void buildShader(GLuint& program, const char* vertSrc, const char* fragSrc,
                            GLint& uProjection, GLint& uOffset, GLint& uSize, GLint& uColor) {
        GLuint vs = compileShader(vertSrc, GL_VERTEX_SHADER);
        GLuint fs = compileShader(fragSrc, GL_FRAGMENT_SHADER);
        if (!vs || !fs) return;
        program = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (program) {
            uProjection = glGetUniformLocation(program, "uProjection");
            uOffset     = glGetUniformLocation(program, "uOffset");
            uSize       = glGetUniformLocation(program, "uSize");
            uColor      = glGetUniformLocation(program, "uColor");
        }
    }

    static void buildLabelShader(GLuint& program, const char* vertSrc, const char* fragSrc,
                                 GLint& uProjection, GLint& uOffset, GLint& uSize, GLint& uTex) {
        GLuint vs = compileShader(vertSrc, GL_VERTEX_SHADER);
        GLuint fs = compileShader(fragSrc, GL_FRAGMENT_SHADER);
        if (!vs || !fs) return;
        program = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (program) {
            uProjection = glGetUniformLocation(program, "uProjection");
            uOffset     = glGetUniformLocation(program, "uOffset");
            uSize       = glGetUniformLocation(program, "uSize");
            uTex        = glGetUniformLocation(program, "uTexture");
        }
    }

    void BossHealthBar::init(int screenWidth, int screenHeight) {
        screenW = static_cast<float>(screenWidth);
        screenH = static_cast<float>(screenHeight);

        float quadVertices[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            0.0f, 1.0f,
            0.0f, 1.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
        };

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);

        buildShader(shaderProgram, bossBarVert, bossBarFrag,
                    uProjectionLoc, uOffsetLoc, uSizeLoc, uColorLoc);

        buildLabelShader(labelShaderProgram, labelVert, labelFrag,
                         lProjectionLoc, lOffsetLoc, lSizeLoc, lTexLoc);
    }

    void BossHealthBar::update(float currentHP, float maxHPVal, float deltaTime) {
        targetHP = currentHP;
        maxHP = maxHPVal;
        // Smooth lerp at speed 3.0 per second
        float delta = targetHP - displayHP;
        if (std::abs(delta) < 0.01f) {
            displayHP = targetHP;
        } else {
            displayHP += delta * 3.0f * deltaTime;
            if ((delta > 0.0f && displayHP > targetHP) || (delta < 0.0f && displayHP < targetHP))
                displayHP = targetHP;
        }
        if (maxHP > 0.0f && displayHP > maxHP) displayHP = maxHP;
        if (displayHP < 0.0f) displayHP = 0.0f;
    }

    void BossHealthBar::render() {
        if (!shaderProgram || !quadVAO) return;

        bool depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindVertexArray(quadVAO);
        glUseProgram(shaderProgram);

        glm::mat4 proj = glm::ortho(0.0f, screenW, screenH, 0.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(uProjectionLoc, 1, GL_FALSE, &proj[0][0]);

        float barW = screenW * 0.6f;
        float barH = 18.0f;
        float barX = (screenW - barW) * 0.5f;
        float barY = 20.0f;

        float fillRatio = (maxHP > 0.0f) ? (displayHP / maxHP) : 0.0f;
        float fillW = barW * fillRatio;

        // Background
        glUniform2f(uOffsetLoc, barX, barY);
        glUniform2f(uSizeLoc, barW, barH);
        glUniform4f(uColorLoc, 0.25f, 0.05f, 0.05f, 0.85f);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Fill
        glUniform2f(uOffsetLoc, barX, barY);
        glUniform2f(uSizeLoc, fillW, barH);
        glUniform4f(uColorLoc, 0.9f, 0.1f, 0.1f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // White 1px border
        glUniform4f(uColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        // Top
        glUniform2f(uOffsetLoc, barX, barY);
        glUniform2f(uSizeLoc, barW, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Bottom
        glUniform2f(uOffsetLoc, barX, barY + barH - 1.0f);
        glUniform2f(uSizeLoc, barW, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Left
        glUniform2f(uOffsetLoc, barX, barY);
        glUniform2f(uSizeLoc, 1.0f, barH);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // Right
        glUniform2f(uOffsetLoc, barX + barW - 1.0f, barY);
        glUniform2f(uSizeLoc, 1.0f, barH);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Label texture (pre-baked "KRAKEN" quad above the bar)
        if (labelTexture != 0 && labelShaderProgram != 0) {
            glUseProgram(labelShaderProgram);
            glUniformMatrix4fv(lProjectionLoc, 1, GL_FALSE, &proj[0][0]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, labelTexture);
            glUniform1i(lTexLoc, 0);

            float labelW = static_cast<float>(labelTexW);
            float labelH = static_cast<float>(labelTexH);
            float labelX = (screenW - labelW) * 0.5f;
            float labelY = barY + barH + 4.0f; // 4px gap above bar

            glUniform2f(lOffsetLoc, labelX, labelY);
            glUniform2f(lSizeLoc, labelW, labelH);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindVertexArray(0);
        glUseProgram(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    void BossHealthBar::resize(int newWidth, int newHeight) {
        screenW = static_cast<float>(newWidth);
        screenH = static_cast<float>(newHeight);
    }

    bool BossHealthBar::loadLabelTexture(const std::string& path) {
        if (labelTexture != 0) {
            glDeleteTextures(1, &labelTexture);
            labelTexture = 0;
        }
        int w, h, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
        if (!data) {
            std::cerr << "[BossHealthBar] Failed to load label texture: " << path << std::endl;
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
        labelTexW = w;
        labelTexH = h;
        return true;
    }

    void BossHealthBar::destroy() {
        if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
        if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
        if (shaderProgram) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
        if (labelShaderProgram) { glDeleteProgram(labelShaderProgram); labelShaderProgram = 0; }
        if (labelTexture) { glDeleteTextures(1, &labelTexture); labelTexture = 0; }
    }

}
