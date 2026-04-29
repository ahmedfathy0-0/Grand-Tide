#version 330 core

in vec4 vs_color;
in vec2 vs_tex_coord;
in vec3 vs_normal;
in vec3 vs_world;

out vec4 frag_color;

uniform float time;

// --- snoise function stays exactly the same, no changes needed ---
float snoise(vec3 uv, float res)
{
    const vec3 s = vec3(1e0, 1e2, 1e3);
    uv *= res;
    vec3 uv0 = floor(mod(uv, res))*s;
    vec3 uv1 = floor(mod(uv+vec3(1.), res))*s;
    vec3 f = fract(uv); f = f*f*(3.0-2.0*f);
    vec4 v = vec4(uv0.x+uv0.y+uv0.z, uv1.x+uv0.y+uv0.z,
                  uv0.x+uv1.y+uv0.z, uv1.x+uv1.y+uv0.z);
    vec4 r = fract(sin(v*1e-1)*1e3);
    float r0 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);
    r = fract(sin((v + uv1.z - uv0.z)*1e-1)*1e3);
    float r1 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);
    return mix(r0, r1, f.z)*2.-1.;
}

void main()
{
    // vs_tex_coord is already 0..1 — remap to -0.5..0.5 like Shadertoy's p
    vec2 p = vs_tex_coord - 0.5;

    // Stretch vertically for a flame shape (taller than wide)
    vec2 fp = vec2(p.x * 1.5, p.y);

    float color = 3.0 - (3.*length(2.*fp));

    vec3 coord = vec3(atan(fp.x, fp.y)/6.2832+.5, length(fp)*.4, .5);

    for(int i = 1; i <= 7; i++)
    {
        float power = pow(2.0, float(i));
        color += (1.5 / power) * snoise(coord + vec3(0., -time*.05, time*.01), power*16.);
    }

    // Brighter fire color — dark red → orange → yellow-white
    vec3 fireColor = vec3(color, pow(max(color,0.),1.5)*0.7, pow(max(color,0.),2.5)*0.3);

    // Much more opaque — minimum alpha of 0.5 in the core
    float alpha = clamp(color, 0.0, 1.0);
    alpha = max(alpha, 0.5);  // Core is always at least 50% opaque
    // Soft edge fade for flame silhouette
    float edgeFade = 1.0 - smoothstep(0.3, 0.5, length(fp));
    alpha *= edgeFade;
    alpha = max(alpha, 0.0);
    if (alpha < 0.03) discard;

    frag_color = vec4(fireColor * 1.3, alpha);
}