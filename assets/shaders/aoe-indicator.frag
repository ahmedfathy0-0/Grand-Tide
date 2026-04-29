#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} vs_in;

out vec4 FragColor;

uniform float time;

void main() {
    // Distance from center of the UV [0,1] space
    float dist = length(vs_in.tex_coord - vec2(0.5));

    // Discard pixels outside the unit circle to make it round
    if (dist > 0.5) discard;

    // Ring: thin band at the edge (radius ~0.5 in UV space)
    float ring = smoothstep(0.44, 0.48, dist) - smoothstep(0.50, 0.54, dist);

    // Pulsing glow
    float pulse = 0.7 + 0.3 * sin(time * 4.0);

    // Orange-red pulsing ring
    vec3 ringColor = vec3(1.0, 0.3, 0.0) * pulse;

    // Soft fill inside the ring for area indication
    float fill = smoothstep(0.5, 0.3, dist) * 0.08;

    FragColor = vec4(ringColor, (ring + fill) * pulse);
}
