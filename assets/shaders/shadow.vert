#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec3 normal;

uniform mat4 lightSpaceMatrix;
uniform mat4 M;

out vec2 vs_tex_coord;

void main() {
    gl_Position = lightSpaceMatrix * M * vec4(position, 1.0);
    vs_tex_coord = tex_coord;
}
