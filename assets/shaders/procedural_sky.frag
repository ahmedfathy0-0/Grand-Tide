#version 330 core

in vec4 vs_color;
in vec2 vs_tex_coord;
in vec3 vs_world; // ray direction

out vec4 frag_color;

uniform float u_time;

#define pi 3.1415926535

// ===== CLOUD / SKY PARAMETERS =====
#define COVERAGE        .50
#define THICKNESS       15.
#define ABSORPTION      1.030725
// Slow down the time uniformly for day/night cycle
#define DAY_NIGHT_SPEED 0.05
#define SKY_TIME (u_time * DAY_NIGHT_SPEED)

// Smooth day/night cycle direction
#define SUN_DIR         normalize(vec3(0, sin(SKY_TIME), -cos(SKY_TIME)))
#define MOON_DIR        normalize(vec3(0, -sin(SKY_TIME), cos(SKY_TIME)))

#define WIND            vec3(0, 0, -u_time * .05) // Slower clouds

#define C_STEPS         25

struct sphere_t { vec3 origin; float radius; int material; };
struct ray_t { vec3 origin; vec3 direction; };
struct hit_t { float t; int material_id; vec3 normal; vec3 origin; };

const float max_dist = 1e8;
const hit_t no_hit = hit_t(max_dist + 1e1, -1, vec3(0), vec3(0));

// Dynamic sun and moon colors based on height
vec3 get_sun_color() {
    float h = max(SUN_DIR.y, 0.0);
    return mix(vec3(1.0, 0.4, 0.2), vec3(1.0, 0.9, 0.8), h);
}
vec3 get_moon_color() {
    return vec3(0.4, 0.5, 0.7); // Cool blue-ish moon
}

const sphere_t atmosphere = sphere_t(vec3(0, -450, 0), 500., 0);

void intersect_sphere(ray_t ray, sphere_t sphere, inout hit_t hit) {
    vec3 rc = sphere.origin - ray.origin;
    float radius2 = sphere.radius * sphere.radius;
    float tca = dot(rc, ray.direction);
    float d2 = dot(rc, rc) - tca * tca;
    if (d2 > radius2) return;
    float thc = sqrt(radius2 - d2);
    float t0 = tca - thc;
    float t1 = tca + thc;
    if (t0 < 0.) t0 = t1;
    if (t0 > hit.t) return;
    hit.t = t0;
    hit.material_id = sphere.material;
    hit.origin = ray.origin + ray.direction * t0;
}

float hash(float n) { return fract(sin(n)*753.5453123); }
float noise(vec3 x) {
    vec3 p = floor(x); vec3 f = fract(x); f = f*f*(3.0 - 2.0*f);
    float n = p.x + p.y*157.0 + 113.0*p.z;
    return mix(mix(mix(hash(n+0.0), hash(n+1.0), f.x), mix(hash(n+157.0), hash(n+158.0), f.x), f.y),
               mix(mix(hash(n+113.0), hash(n+114.0), f.x), mix(hash(n+270.0), hash(n+271.0), f.x), f.y), f.z);
}
float fbm(vec3 pos, float lacunarity) {
    vec3 p = pos;
    float t  = 0.51749673 * noise(p); p *= lacunarity;
    t += 0.25584929 * noise(p); p *= lacunarity;
    t += 0.12527603 * noise(p); p *= lacunarity;
    t += 0.06255931 * noise(p);
    return t;
}
vec3 render_sky_color(vec3 rd) {
    // Background sky gradients (day and night blending)
    vec3 day_sky = mix(vec3(0.5, 0.7, 0.9), vec3(0.1, 0.3, 0.8), clamp(1.0 - rd.y, 0.0, 1.0));
    vec3 night_sky = vec3(0.01, 0.02, 0.1); 
    
    float time_blend = smoothstep(-0.2, 0.2, SUN_DIR.y);
    vec3 sky = mix(night_sky, day_sky, time_blend);

    // Starry night background
    if(time_blend < 1.0 && rd.y > 0.0) {
        float stars = pow(hash(dot(rd, vec3(12.9898, 78.233, 45.164))), 500.0);
        sky += vec3(stars) * (1.0 - time_blend) * 3.0; // Show stars more prominently at night
    }

    // Add sun disk and halo
    float sun_amount = max(dot(rd, SUN_DIR), 0.0);
    // Add sun bloom and disk
    vec3 sun_col = get_sun_color();
    float sun_bloom = pow(sun_amount, 8.0) * 0.5;
    float sun_disk = pow(sun_amount, 1000.0) * 5.0; // The actual disc
    sky += sun_col * min((sun_bloom + sun_disk), 1.0) * smoothstep(-0.1, 0.1, SUN_DIR.y);

    // Add moon disk and halo (drawn from the ShaderToy inspiration)
    float moon_amount = max(dot(rd, MOON_DIR), 0.0);
    vec3 moon_col = get_moon_color();
    float moon_bloom = pow(moon_amount, 21.0) * 0.43;

    // Draw the moon disc, incorporating some simple procedural noise for craters
    if (moon_amount > 0.999) { // Inside moon angular radius
        vec2 moon_uv = rd.xz; // Very rough mapping
        float crater_noise = noise(rd * 150.0) * 0.3 + 0.7; // Simple crater-like variation
        sky += moon_col * 3.0 * crater_noise * smoothstep(-0.1, 0.1, MOON_DIR.y);
    } else {
        sky += moon_col * min(moon_bloom, 1.0) * smoothstep(-0.1, 0.1, MOON_DIR.y);
    }

    return sky;
}
float density(vec3 pos, vec3 offset, float t) {
    vec3 p = pos * .0212242 + offset;
    float FBM_FREQ = 2.76434;
    float dens = fbm(p, FBM_FREQ);
    float cov = 1. - COVERAGE;
    dens *= smoothstep(cov, cov + .05, dens);
    return clamp(dens, 0., 1.); 
}
vec4 render_clouds(ray_t eye) {
    hit_t hit = no_hit;
    intersect_sphere(eye, atmosphere, hit);
    if (hit.material_id == -1) return vec4(0.0); 
    
    float march_step = THICKNESS / float(C_STEPS);
    vec3 dir_step = eye.direction / eye.direction.y * march_step;
    vec3 pos = hit.origin;

    float T = 1.; vec3 C = vec3(0); float alpha = 0.;
    for (int i = 0; i < C_STEPS; i++) {
        float h = float(i) / float(C_STEPS);
        float dens = density(pos, WIND, h);

        float T_i = exp(-ABSORPTION * dens * march_step);
        T *= T_i;
        if (T < .01) break;

        C += T * (exp(h) / 1.75) * dens * march_step;
        alpha += (1. - T_i) * (1. - alpha);

        pos += dir_step;
        if (length(pos) > 1e3) break;
    }
    return vec4(C, alpha);
}

vec3 sky_eval(vec3 rd) {
    vec3 sky_base = render_sky_color(rd);
    if(rd.y > 0.0){
        ray_t eye_ray = ray_t(vec3(0.0, 1.0, 0.0), rd);
        vec4 cld = render_clouds(eye_ray);
        return mix(sky_base, cld.rgb/(0.000001+cld.a), cld.a);
    } else {
        return mix(vec3(0.1, 0.2, 0.3), vec3(0.0, 0.1, 0.4), -rd.y);
    }
}

uniform vec3 camera_position;

void main() {
    float iTime = u_time;
    // We already have the ray direction directly since it's mapped to a sphere!
    vec3 rd = normalize(vs_world);
    
    vec3 col = sky_eval(rd);
    col = clamp(col, 0.0, 1.0);
    col = pow(col, vec3(0.87)); // gamma correction
    
    frag_color = vec4(col, 1.0);
}
