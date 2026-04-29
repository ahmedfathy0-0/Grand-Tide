#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} vs_in;

out vec4 FragColor;

uniform float time;
uniform float burnIntensity;

// Simple hash-based noise for fire effect (no texture needed)
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

void main() {
    // Discard pixels outside the unit circle to make it round
    float dist = length(vs_in.tex_coord - vec2(0.5));
    if (dist > 0.5) discard;

    vec2 uv = vs_in.tex_coord;

    // Scroll UVs upward to simulate rising flames
    uv.y -= time * 0.8;

    float n1 = fbm(uv * 4.0);
    float n2 = fbm(uv * 8.0 + vec2(5.0, 3.0));
    float flame = n1 * n2;

    // Fire color gradient: dark red -> orange -> yellow
    vec3 fireColor = mix(vec3(0.8, 0.1, 0.0),
                    mix(vec3(1.0, 0.4, 0.0),
                        vec3(1.0, 0.9, 0.2),
                        flame),
                    flame);

    // Fade out at edges
    float edgeFade = 1.0 - smoothstep(0.3, 0.5, length(vs_in.tex_coord - vec2(0.5)));

    float alpha = flame * burnIntensity * edgeFade;
    if (alpha < 0.02) discard;

    FragColor = vec4(fireColor, alpha);
}
