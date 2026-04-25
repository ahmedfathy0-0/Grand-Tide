#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec3 normal;

out vec4 vs_color;
out vec2 vs_tex_coord;
out vec3 vs_normal;
out vec3 vs_world;
out vec4 vs_light_space_pos;

uniform mat4 M;
uniform mat4 M_IT;
uniform mat4 VP;
uniform mat4 lightSpaceMatrix;

void main() {
    vec4 world_position = M * vec4(position, 1.0);
    gl_Position = VP * world_position;
    
    vs_color = color;
    vs_tex_coord = tex_coord;
    
    // Transform normal, removing translation and handling non-uniform scale
    vs_normal = normalize((M_IT * vec4(normal, 0.0)).xyz);
    vs_world = world_position.xyz;
    
    vs_light_space_pos = lightSpaceMatrix * world_position;
}