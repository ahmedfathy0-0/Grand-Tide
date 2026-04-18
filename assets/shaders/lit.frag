#version 330 core

in vec4 vs_color;
in vec2 vs_tex_coord;
in vec3 vs_normal;
in vec3 vs_world;

out vec4 frag_color;

struct Light {
    int type; // 0: directional, 1: point
    vec3 color;
    float intensity;
    vec3 position; // Direction for directional, Position for point
};

const int MAX_LIGHTS = 16;
uniform Light lights[MAX_LIGHTS];
uniform int light_count;

uniform vec3 camera_position;

uniform sampler2D albedo;
uniform sampler2D specular;
uniform sampler2D roughness;

uniform vec3 albedo_tint;
uniform vec3 specular_tint;
uniform float roughness_multiplier;

void main() {
    vec3 normal = normalize(vs_normal);
    vec3 view_dir = normalize(camera_position - vs_world);
    
    vec4 albedo_color = texture(albedo, vs_tex_coord) * vec4(albedo_tint, 1.0) * vs_color;
    vec3 specular_color = texture(specular, vs_tex_coord).rgb * specular_tint;
    float rgh = texture(roughness, vs_tex_coord).r * roughness_multiplier;
    
    // Map roughness [0, 1] to shininess power
    float shininess = mix(1024.0, 1.0, clamp(rgh, 0.0, 1.0));

    // Hemisphere Ambient Lighting (Outdoor daylight fill)
    vec3 sky_light = vec3(0.4, 0.5, 0.7);    // Blueish scattered light from the sky
    vec3 ground_light = vec3(0.1, 0.1, 0.1); // Dim light bouncing from the ground
    vec3 ambient_color = mix(ground_light, sky_light, normal.y * 0.5 + 0.5);
    
    vec3 ambient = ambient_color * albedo_color.rgb;
    vec3 final_color = ambient;

    for(int i = 0; i < light_count; ++i) {
        vec3 light_color = lights[i].color * lights[i].intensity;
        
        vec3 light_dir;
        float attenuation = 1.0;
        
        if (lights[i].type == 0) { // Directional
            light_dir = normalize(-lights[i].position);
        } else { // Point
            light_dir = lights[i].position - vs_world;
            float distance = length(light_dir);
            light_dir = normalize(light_dir);
            // Basic attenuation
            attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
        }
        
        vec3 half_vector = normalize(light_dir + view_dir);
        
        float diff = max(dot(normal, light_dir), 0.0);
        vec3 diffuse = diff * albedo_color.rgb;
        
        float spec = pow(max(dot(normal, half_vector), 0.0), shininess);
        vec3 reflect_specular = spec * specular_color;
        
        final_color += (diffuse + reflect_specular) * light_color * attenuation;
    }

    frag_color = vec4(final_color, albedo_color.a);
}