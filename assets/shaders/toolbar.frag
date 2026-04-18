#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} fs_in;

out vec4 fragColor;

uniform float time;
uniform bool isSelected;
uniform vec3 baseColor; // e.g., vec3(0.2, 0.2, 0.2) for dark glass

// Function to create a rounded rectangle
float roundedRect(vec2 p, vec2 size, float radius) {
    vec2 d = abs(p) - size + radius;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - radius;
}

void main() {
    // Center the coordinates to (-0.5, 0.5)
    vec2 TexCoords = fs_in.tex_coord;
    vec2 p = TexCoords - 0.5;
    
    // 1. Calculate the Shape
    float d = roundedRect(p, vec2(0.45, 0.45), 0.1);
    
    // 2. Base Alpha (Transparency)
    float mask = smoothstep(0.01, 0.0, d);
    if (mask < 0.1) discard; // Don't draw outside the rounded rect

    // 3. Create a Glassy Gradient
    float gradient = TexCoords.y * 0.2;
    vec3 color = baseColor + gradient;

    // 4. Add a Border
    float borderMask = smoothstep(0.0, 0.02, abs(d + 0.01));
    vec3 borderColor = isSelected ? vec3(1.0, 0.8, 0.0) : vec3(0.5); // Yellow glow if selected
    color = mix(borderColor, color, borderMask);

    // 5. Add a "Selection Pulse"
    if (isSelected) {
        float pulse = 0.5 + 0.5 * sin(time * 5.0);
        color += borderColor * pulse * 0.2;
    }

    // 6. Subtle Reflection (The "Glass" look)
    float reflection = smoothstep(0.4, 0.41, TexCoords.y + p.x);
    color += reflection * 0.05;

    fragColor = vec4(color, 0.8 * mask); // 0.8 Alpha for frosted glass look
}
