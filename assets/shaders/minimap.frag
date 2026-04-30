#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} fs_in;

out vec4 fragColor;

uniform float time;
uniform vec2 entities_pos[32]; // Max 32 entities on radar for now
uniform float entities_type[32]; // 0.0 = enemy, 1.0 = fish, 2.0 = wood
uniform int num_entities;

#define s(v) smoothstep(0.01, 0.005, abs(x-v))
#define l(x,y) length(c - vec2(x,y))

void main()
{
    // Map tex_coord from [0, 1] to [-1, 1], and flip Y to match UI
    vec2 c = fs_in.tex_coord * 2.0 - 1.0;
    c.y = -c.y;
    
    vec2 k = 1.0 - step(0.007, abs(c));
    
    // Polar coordinates for radar sweep
    float x = l(0.0, 0.0); // distance from center
    float y = mod(atan(c.y, c.x) + time, 6.2831853);
    
    // Radar sweep visual
    float d = max(0.75 - y * 0.4, 0.0);
    
    // Targets (blips) - separate by type
    float bRed = 1.0;
    float bBlue = 1.0;
    float bBrown = 1.0;
    
    for (int i = 0; i < num_entities && i < 32; ++i) {
        float blip = length(c - entities_pos[i]);
        if (entities_type[i] < 0.5) bRed = min(bRed, blip);
        else if (entities_type[i] < 1.5) bBlue = min(bBlue, blip);
        else bBrown = min(bBrown, blip);
    }
    
    bRed = bRed + 0.06 - y * 0.04;
    bBlue = bBlue + 0.06 - y * 0.04;
    bBrown = bBrown + 0.06 - y * 0.04;

    float blipIntensityRed = y < 4.5 && bRed < 0.08 ? bRed * (18.0 - 4.0 * y) : 0.0;
    float blipIntensityBlue = y < 4.5 && bBlue < 0.08 ? bBlue * (18.0 - 4.0 * y) : 0.0;
    float blipIntensityBrown = y < 4.5 && bBrown < 0.08 ? bBrown * (18.0 - 4.0 * y) : 0.0;

    vec4 O = vec4(
        // R channel: red blips + brown blips + base tint
        blipIntensityRed + blipIntensityBrown * 0.65 + 0.1,
        
        // G channel: radar background & sweep + brown blips
        (x < 0.9 ? 
            0.25 // background
            + 0.1 * (s(0.67) + s(0.43) + s(0.2) + k.x + k.y) // grid
            + d * d // detector (sweep fade)
            + max(0.4 - abs(y * (x * 50.0 + 0.3) - 0.4), 0.0) // front of ray
        : 0.0)
        // Outer border (added to Green to make a green border, or we can make it white)
        + max(0.0, 1.0 - abs(l(0.0,0.0) - 0.9) * 50.0)
        + blipIntensityBrown * 0.4,
        
        // B channel: blue blips + border
        0.1 + max(0.0, 1.0 - abs(l(0.0,0.0) - 0.9) * 50.0) + blipIntensityBlue,
        
        // Alpha: discard pixels outside the border entirely for clean UI
        1.0
    );

    // Alpha masking: make it circular and transparent outside
    if (x > 0.95) {
        discard;
    }

    fragColor = O;
}
