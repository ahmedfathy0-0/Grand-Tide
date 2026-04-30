#version 330 core

in vec2 vs_tex_coord;

uniform sampler2D albedo;
uniform bool has_albedo;
uniform float alphaThreshold;

void main() {
    // Optional alpha test for shadows on foliage
    if(has_albedo) {
        if(texture(albedo, vs_tex_coord).a < alphaThreshold) {
            discard;
        }
    }
}
