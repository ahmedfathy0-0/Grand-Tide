#version 330 core

in vec4 vs_color;
in vec2 vs_tex_coord;
in vec3 vs_normal;
in vec3 vs_world;
in vec4 vs_light_space_pos;

out vec4 frag_color;

struct Light {
    int type; // 0: directional, 1: point, 2: spot
    vec3 color;
    float intensity;
    vec3 position; // Direction for directional, Position for point
    vec3 direction;
    vec2 cone_angles;
};

const int MAX_LIGHTS = 16;
uniform Light lights[MAX_LIGHTS];
uniform int light_count;

uniform vec3 camera_position;
uniform float u_time;

uniform sampler2D albedo;
uniform sampler2D specular;   // Legacy, interpreted as metallic or generic specular tint
uniform sampler2D roughness;
// The shadow map
uniform sampler2D shadow_map;

uniform vec3 albedo_tint;
uniform vec3 specular_tint;
// For PBR: Use roughness_multiplier directly as Roughness if no texture exists, or multiply it.
uniform float roughness_multiplier;

// ----- PROCEDURAL SKY FOR IBL -----
#define DAY_NIGHT_SPEED 0.05
#define SKY_TIME (u_time * DAY_NIGHT_SPEED)
#define SUN_DIR normalize(vec3(0, sin(SKY_TIME), -cos(SKY_TIME)))
#define MOON_DIR normalize(vec3(0, -sin(SKY_TIME), cos(SKY_TIME)))

float hash(float n) { return fract(sin(n)*753.5453123); }
vec3 get_sun_color() {
    float h = max(SUN_DIR.y, 0.0);
    return mix(vec3(1.0, 0.4, 0.2), vec3(1.0, 0.9, 0.8), h);
}
vec3 get_moon_color() {
    return vec3(0.4, 0.5, 0.7);
}
vec3 render_sky_color(vec3 rd) {
    vec3 day_sky = mix(vec3(0.5, 0.7, 0.9), vec3(0.1, 0.3, 0.8), clamp(1.0 - rd.y, 0.0, 1.0));
    vec3 night_sky = vec3(0.01, 0.02, 0.1); 
    float time_blend = smoothstep(-0.2, 0.2, SUN_DIR.y);
    vec3 sky = mix(night_sky, day_sky, time_blend);
    if(time_blend < 1.0 && rd.y > 0.0) {
        float stars = pow(hash(dot(rd, vec3(12.9898, 78.233, 45.164))), 500.0);
        sky += vec3(stars) * (1.0 - time_blend) * 3.0; 
    }
    float sun_amount = max(dot(rd, SUN_DIR), 0.0);
    vec3 sun_col = get_sun_color();
    float sun_bloom = pow(sun_amount, 8.0) * 0.5;
    float sun_disk = pow(sun_amount, 1000.0) * 5.0; 
    sky += sun_col * min((sun_bloom + sun_disk), 1.0) * smoothstep(-0.1, 0.1, SUN_DIR.y);

    float moon_amount = max(dot(rd, MOON_DIR), 0.0);
    vec3 moon_col = get_moon_color();
    float moon_bloom = pow(moon_amount, 21.0) * 0.43;
    sky += moon_col * min(moon_bloom, 1.0) * smoothstep(-0.1, 0.1, MOON_DIR.y);

    return sky;
}

// ----- PBR UTILS -----
const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ----- SHADOW CALCULATION -----
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // No shadows if outside light frustum
    if(projCoords.z > 1.0)
        return 0.0;

    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadow_map, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // calculate bias (based on depth map resolution and slope)
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadow_map, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadow_map, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    return shadow;
}

void main() {
    vec3 N = normalize(vs_normal);
    vec3 V = normalize(camera_position - vs_world);
    
    // Legacy maps extraction to PBR
    vec4 albedo_sample_raw = texture(albedo, vs_tex_coord);
    vec3 albedo_color = pow(albedo_sample_raw.rgb, vec3(2.2)) * albedo_tint * vs_color.rgb; // Convert to linear space
    
    // If user's specular map is active, we can approximate metallic from specular intensity
    vec3 spec_sample = texture(specular, vs_tex_coord).rgb;
    float metallic = max(spec_sample.r, max(spec_sample.g, spec_sample.b)) * max(specular_tint.r, 0.0) * 5.0; // simple metallic guess mapped to specular intensity
    metallic = clamp(metallic, 0.0, 1.0);
    
    float rough = texture(roughness, vs_tex_coord).r;
    rough = clamp(rough * roughness_multiplier, 0.05, 1.0); // Never go to pure zero to avoid divisions by zero
    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo_color, metallic);
    
    vec3 Lo = vec3(0.0);
    
    for(int i = 0; i < light_count; ++i) {
        vec3 light_color = lights[i].color * lights[i].intensity;
        vec3 L;
        float attenuation = 1.0;
        float shadow = 0.0;
        
        if (lights[i].type == 0) { // Directional
            L = normalize(-lights[i].position);
            // Dynamic shadow map is mapped to Light 0 (The Sun/Moon)
            if (i == 0) {
                shadow = ShadowCalculation(vs_light_space_pos, N, L);
            }
        } else if (lights[i].type == 1) { // Point
            L = lights[i].position - vs_world;
            float distance = length(L);
            L = normalize(L);
            attenuation = 1.0 / (distance * distance); // Physical inverse square falloff
        } else if (lights[i].type == 2) { // Spot
            L = lights[i].position - vs_world;
            float distance = length(L);
            L = normalize(L);
            // Long distance reach attenuation
            attenuation = 1.0 / (1.0 + 0.07 * distance + 0.0002 * distance * distance);
            float theta = dot(L, normalize(-lights[i].direction));
            float inner = lights[i].cone_angles.x * 2.5;
            float outer = lights[i].cone_angles.y * 2.5;
            float epsilon = cos(inner) - cos(outer);
            float spot_intensity = clamp((theta - cos(outer)) / epsilon, 0.0, 1.0);
            attenuation *= spot_intensity;
        }
        
        vec3 H = normalize(V + L);
        vec3 radiance = light_color * attenuation;
        
        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, rough);
        float G   = GeometrySmith(N, V, L, rough);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular_brdf = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        float NdotL = max(dot(N, L), 0.0);
        
        Lo += (1.0 - shadow) * (kD * albedo_color / PI + specular_brdf) * radiance * NdotL;
    }
    
    // ----- IBL (Ambient lighting) from Procedural Sky -----
    vec3 kS = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, rough);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    
    // Diffuse Irradiance is approximated by the sky color directly above the normal
    vec3 irradiance = render_sky_color(N);
    // Multiply by ground color mix to reduce undersides
    vec3 ground_color = vec3(0.04, 0.04, 0.04);
    irradiance = mix(ground_color, irradiance, max(N.y*0.5+0.5, 0.0));
    vec3 diffuse = irradiance * albedo_color;
    
    // Specular IBL approximated by reflecting V across N and sampling the sky
    vec3 R = reflect(-V, N);
    // Simulate roughness by mixing towards diffuse irradiance
    vec3 prefilteredColor = mix(render_sky_color(R), irradiance, rough); 
    // Usually combined with BRDF integration map, but we'll approximate:
    vec2 envBRDF = vec2(1.0, 0.0); // simple hack for constant fresnel multiplier
    vec3 specular_ibl = prefilteredColor * (kS * envBRDF.x + envBRDF.y);
    
    vec3 ambient = (kD * diffuse + specular_ibl);
    
    vec3 color = ambient + Lo;
    
    // HDR tonemapping (ACES approximation or similar)
    // Using simple reinhard for now since sky handles its own:
    color = color / (color + vec3(1.0));
    // Gamma correct
    color = pow(color, vec3(1.0/2.2)); 

    frag_color = vec4(color, albedo_sample_raw.a);
}