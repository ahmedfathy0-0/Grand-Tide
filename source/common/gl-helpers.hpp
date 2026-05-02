#pragma once

#include <glad/gl.h>
#include <stb/stb_image.h>
#include <iostream>

namespace our {

    // Compile a GL shader program from vertex + fragment source strings.
    // Returns the linked program ID, or 0 on failure.
    inline GLuint compileShaderProgram(const char* vertSrc, const char* fragSrc) {
        auto compile = [](const char* src, GLenum type) -> GLuint {
            GLuint sh = glCreateShader(type);
            glShaderSource(sh, 1, &src, nullptr);
            glCompileShader(sh);
            int ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char buf[512]; glGetShaderInfoLog(sh, 512, nullptr, buf);
                std::cerr << "[GL] shader compile error: " << buf << std::endl;
                glDeleteShader(sh); return 0;
            }
            return sh;
        };
        GLuint vs = compile(vertSrc, GL_VERTEX_SHADER);
        GLuint fs = compile(fragSrc, GL_FRAGMENT_SHADER);
        if (!vs || !fs) { if(vs) glDeleteShader(vs); if(fs) glDeleteShader(fs); return 0; }
        GLuint p = glCreateProgram();
        glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
        int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char buf[512]; glGetProgramInfoLog(p, 512, nullptr, buf);
            std::cerr << "[GL] program link error: " << buf << std::endl;
            glDeleteProgram(p); p = 0;
        }
        glDeleteShader(vs); glDeleteShader(fs);
        return p;
    }

    // Create a fullscreen quad VAO (6 vertices, 2 triangles).
    // Fills the given VBO reference. Returns the VAO.
    inline GLuint createQuadVAO(GLuint& vbo) {
        float quad[12] = { 0,0, 1,0, 1,1, 0,0, 1,1, 0,1 };
        GLuint vao;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
        return vao;
    }

    // Load an image file into a GL texture (RGBA). Returns texture ID, or 0 on failure.
    inline GLuint loadTexture(const char* path) {
        int w, h, ch;
        unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
        if (!data) { std::cerr << "[GL] Failed to load texture: " << path << std::endl; return 0; }
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    }

    // Shared vertex shader for fullscreen quad overlays (menu, video, etc.)
    static constexpr const char* quadVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
void main() {
    vUV = vec2(aPos.x, 1.0 - aPos.y);
    gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
}
)";

} // namespace our
