#version 330 core

layout(location = 0) in vec3 position;

out vec3 vs_world;

uniform mat4 transform;

void main() {
    gl_Position = transform * vec4(position, 1.0);
    // For a unit sphere, the local position is the direction!
    vs_world = position;
}