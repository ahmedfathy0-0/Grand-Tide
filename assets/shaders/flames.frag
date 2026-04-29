#version 330 core

in vec2 vs_tex_coord;
in vec3 vs_normal;
in vec3 vs_world;
in vec4 vs_color;

out vec4 frag_color;

uniform float time;

#define FLAMECOLOR vec3(50.0, 5.0, 1.0)
#define PI 3.14159

float FlameShape(vec2 uv) {
    float flameControl1 = 5.;
    float flameControl2 = 1.5;
    
    float a = mod(atan(uv.x, uv.y + 2.), PI * 2.) / flameControl1 - PI / flameControl1;
    float angle = PI - (.5 + .25);
    float d = length(uv - vec2(0., -2.)) * sin(angle + abs(a));
    return smoothstep(0., flameControl2, d);
}

mat2 Rot(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c, -s, s, c);
}

float R21(vec2 p) {
    return fract(sin(dot(p.xy, vec2(2.3245, 5.234))) * 123.5632145);
}

float NoiseValue(vec2 uv) {
    vec2 gv = fract(uv);
    vec2 id = floor(uv);
    
    gv = gv * gv * (3. - 2. * gv);
    float a = R21(id);
    float b = R21(id + vec2(1., 0.));
    float c = R21(id + vec2(0., 1.));
    float d = R21(id + vec2(1., 1.));
    return mix(a, b, gv.x) + (c - a) * gv.y * (1. - gv.x) + (d - b) * gv.x * gv.y;
}

float SmoothNoise(vec2 uv) {
    float value = 0.;
    float amplitude = .5;
    for (int i = 0; i < 8; i++) {
        value += NoiseValue(uv) * amplitude;
        uv *= 2.;
        amplitude *= .5;
    }
    return value;
}

vec3 Flame(vec2 uv) {
    uv *= 6.;
    
    vec3 col = vec3(0.);
    float shape = FlameShape(uv);
    uv *= Rot(2.5);
    
    vec2 rn = vec2(0.);
    // iTime replaced with your engine's time uniform
    rn.x = SmoothNoise(uv + 1.984 + 4.5 * time);
    rn.y = SmoothNoise(uv + 1.    + 4.5 * time);
    
    col += SmoothNoise(uv + rn * 2.5);
    col -= shape;
    
    col = col / (1.5 + col);
    col = max(col, vec3(0.0)); // clamp before pow to prevent NaN
    col = pow(col, vec3(3. / 2.2));
    col *= FLAMECOLOR;
    
    return col;
}

void main()
{
    vec2 uv = vs_tex_coord - 0.5;
    uv.y *= 1.5; // stretch vertically for flame shape

    vec3 col = Flame(uv);
    col *= 2.0;

    // Use brightness as alpha — black = transparent, bright = opaque
    float alpha = clamp(dot(col, vec3(0.212, 0.715, 0.072)), 0.0, 1.0);

    // Discard nearly black pixels completely
    if (alpha < 0.05) discard;

    frag_color = vec4(col, alpha);
}