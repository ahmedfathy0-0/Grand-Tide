#version 330 core

in vec4 vs_color;
in vec2 vs_tex_coord;
in vec3 vs_normal;
in vec3 vs_world;
in vec4 vs_light_space_pos;

out vec4 frag_color;

struct Light {
    int   type;        // 0: directional, 1: point, 2: spot
    vec3  color;
    float intensity;
    vec3  position;    // Direction for directional, Position for point/spot
    vec3  direction;   // Used by spot lights
    vec2  cone_angles; // (inner, outer) half-angles for spot
};

const int MAX_LIGHTS = 16;
uniform Light lights[MAX_LIGHTS];
uniform int   light_count;

uniform vec3  camera_position;
uniform float u_time;

uniform sampler2D albedo;
uniform sampler2D specular;   // Interpreted as metallic/specular tint map
uniform sampler2D roughness;
uniform sampler2D shadow_map; // PCF shadow map for light[0]

uniform vec3  albedo_tint;
uniform vec3  specular_tint;
uniform float roughness_multiplier;

// Wetness: 0.0 = dry, 1.0 = fully wet / dripping
uniform float u_wetness;

// ----- PROCEDURAL SKY FOR IBL -----
#define DAY_NIGHT_SPEED 0.05
#define SKY_TIME        (u_time * DAY_NIGHT_SPEED)
#define SUN_DIR         normalize(vec3(0.0, sin(SKY_TIME), -cos(SKY_TIME)))
#define MOON_DIR        normalize(vec3(0.0, -sin(SKY_TIME),  cos(SKY_TIME)))

float hash(float n) { return fract(sin(n) * 753.5453123); }

vec3 get_sun_color() {
    float h = max(SUN_DIR.y, 0.0);
    return mix(vec3(1.0, 0.4, 0.2), vec3(1.0, 0.9, 0.8), h);
}
vec3 get_moon_color() { return vec3(0.4, 0.5, 0.7); }

vec3 render_sky_color(vec3 rd) {
    vec3  day_sky   = mix(vec3(0.5, 0.7, 0.9), vec3(0.1, 0.3, 0.8), clamp(1.0 - rd.y, 0.0, 1.0));
    vec3  night_sky = vec3(0.01, 0.02, 0.1);
    float time_blend = smoothstep(-0.2, 0.2, SUN_DIR.y);
    vec3  sky = mix(night_sky, day_sky, time_blend);

    vec3  sun_col   = get_sun_color();
    float sun_amount = max(dot(rd, SUN_DIR), 0.0);
    float sun_bloom  = pow(sun_amount,  8.0) * 0.5;
    float sun_disk   = pow(sun_amount, 1000.0) * 5.0;
    sky += sun_col * min((sun_bloom + sun_disk), 1.0) * smoothstep(-0.1, 0.1, SUN_DIR.y);

    vec3  moon_col    = get_moon_color();
    float moon_amount = max(dot(rd, MOON_DIR), 0.0);
    float moon_bloom  = pow(moon_amount, 21.0) * 0.43;
    sky += moon_col * min(moon_bloom, 1.0) * smoothstep(-0.1, 0.1, MOON_DIR.y);

    return sky;
}

// ----- PBR UTILS -----
const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ----- SHADOW CALCULATION (3x3 PCF) -----
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.z > 1.0) return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadow_map, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadow_map, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    vec3 N = normalize(vs_normal);
    vec3 V = normalize(camera_position - vs_world);

    // ---- Sample raw textures ----
    vec4 albedo_sample_raw = texture(albedo,    vs_tex_coord);
    vec3 spec_sample       = texture(specular,  vs_tex_coord).rgb;
    float rough_sample     = texture(roughness, vs_tex_coord).r;

    // ---- Convert albedo to linear space ----
    vec3 albedo_lin = pow(albedo_sample_raw.rgb, vec3(2.2)) * albedo_tint * vs_color.rgb;

    // ---- Roughness ----
    float rough = clamp(rough_sample * roughness_multiplier, 0.05, 1.0);

    // ---- Metallic (approximated from specular map intensity) ----
    float metallic = clamp(
        max(spec_sample.r, max(spec_sample.g, spec_sample.b)) * max(specular_tint.r, 0.0) * 5.0,
        0.0, 1.0);

    // ---- Wetness effect ----
    // Scrolling drip drops using world-space position and time
   
    // ---- PBR base reflectivity (F0) ----
    vec3 F0 = mix(vec3(0.04), albedo_lin, metallic);

    // ---- Direct lighting loop ----
    vec3 Lo = vec3(0.0);

    for(int i = 0; i < light_count; ++i) {
        vec3  light_color = lights[i].color * lights[i].intensity;
        vec3  L;
        float attenuation = 1.0;
        float shadow      = 0.0;

        if (lights[i].type == 0) { // Directional
            L = normalize(-lights[i].position);
            if (i == 0) {
                shadow = ShadowCalculation(vs_light_space_pos, N, L);
            }
        } else if (lights[i].type == 1) { // Point
            L = lights[i].position - vs_world;
            float dist = length(L);
            L = normalize(L);
            attenuation = 1.0 / (dist * dist); // Physical inverse-square falloff
        } else if (lights[i].type == 2) { // Spot
            L = lights[i].position - vs_world;
            float dist = length(L);
            L = normalize(L);
            attenuation = 1.0 / (1.0 + 0.07 * dist + 0.0002 * dist * dist);
            float theta        = dot(L, normalize(-lights[i].direction));
            float inner        = lights[i].cone_angles.x * 2.5;
            float outer        = lights[i].cone_angles.y * 2.5;
            float epsilon      = cos(inner) - cos(outer);
            float spot_factor  = clamp((theta - cos(outer)) / epsilon, 0.0, 1.0);
            attenuation *= spot_factor;
        }

        vec3  H        = normalize(V + L);
        vec3  radiance = light_color * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, rough);
        float G   = GeometrySmith(N, V, L, rough);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular_brdf = numerator / denominator;

        vec3  kS    = F;
        vec3  kD    = (vec3(1.0) - kS) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);

        Lo += (1.0 - shadow) * (kD * albedo_lin / PI + specular_brdf) * radiance * NdotL;
    }

    // ---- Ambient / IBL from procedural sky ----
    vec3 kS_ibl = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, rough);
    vec3 kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);

    vec3 irradiance    = render_sky_color(N);
    vec3 ground_color  = vec3(0.04, 0.04, 0.04);
    irradiance         = mix(ground_color, irradiance, max(N.y * 0.5 + 0.5, 0.0));
    vec3 diffuse_ibl   = irradiance * albedo_lin;

    vec3 R               = reflect(-V, N);
    vec3 prefilteredColor = mix(render_sky_color(R), irradiance, rough);
    vec3 specular_ibl    = prefilteredColor * kS_ibl; // simplified BRDF integration

    vec3 ambient = kD_ibl * diffuse_ibl + specular_ibl;

    vec3 color = ambient + Lo;

    // HDR tonemapping (Reinhard)
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    frag_color = vec4(color, albedo_sample_raw.a);
}
