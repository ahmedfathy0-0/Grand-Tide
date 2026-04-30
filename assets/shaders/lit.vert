#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec3 normal;
layout(location = 4) in ivec4 boneIds;
layout(location = 5) in vec4 weights;

out vec4 vs_color;
out vec2 vs_tex_coord;
out vec3 vs_normal;
out vec3 vs_world;
out vec4 vs_light_space_pos;
out float gl_ClipDistance[1];

uniform mat4 M;
uniform mat4 M_IT;
uniform mat4 VP;
uniform mat4 lightSpaceMatrix;

// Increased from 128 -> 200 to support larger rigs
const int MAX_BONES = 200;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform bool isAnimated;

// UV tiling/scrolling multiplier (default = no change)
uniform vec2 uv_multiplier = vec2(1.0, 1.0);

// Clip plane (e.g. for water reflections/refractions)
uniform float uClipPlaneY;
uniform bool  uUseClipPlane;

void main() {
    mat4 boneTransform = mat4(1.0);
    if(isAnimated) {
        mat4 bT = mat4(0.0);
        bool hasBones = false;
        for(int i = 0; i < MAX_BONE_INFLUENCE; i++) {
            if(boneIds[i] >= 0 && boneIds[i] < MAX_BONES) {
                bT += finalBonesMatrices[boneIds[i]] * weights[i];
                hasBones = true;
            }
        }
        if(hasBones) boneTransform = bT;
    }

    vec4 localPosition  = boneTransform * vec4(position, 1.0);
    vec4 world_position = M * localPosition;
    gl_Position         = VP * world_position;

    vs_color    = color;
    vs_tex_coord = tex_coord * uv_multiplier;

    // Transform normal with inverse-transpose to handle non-uniform scale
    mat3 normalMatrix = mat3(M_IT);
    if(isAnimated) {
        normalMatrix = mat3(M_IT) * mat3(boneTransform);
    }
    vs_normal = normalize(normalMatrix * normal);
    vs_world  = world_position.xyz;

    // Pass position in light space for shadow mapping
    vs_light_space_pos = lightSpaceMatrix * world_position;

    // Clip plane (used for water reflections / refractions)
    if (uUseClipPlane) {
        gl_ClipDistance[0] = world_position.y - uClipPlaneY;
    } else {
        gl_ClipDistance[0] = 1.0;
    }
}
