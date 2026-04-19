#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} fs_in;

out vec4 fragColor;

uniform float time;
uniform vec2 raft_relative_pos;

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
    
    // Targets (blips)
    // The player is at the center, we draw a small dot for the player
    float player_blip = l(0.0, 0.0);
    
    // The raft is at raft_relative_pos
    // Clamp the raft position so it doesn't go completely off-screen, or let it disappear
    // We'll just draw it where it is
    float raft_blip = length(c - raft_relative_pos);
    
    // Combine targets: player (center) and raft
    float b = min(player_blip, raft_blip) + 0.06 - y * 0.04;

    vec4 O = vec4(
        // R channel: targets (red blips)
        y < 1.5 && b < 0.08 ? b * (18.0 - 13.0 * y) : 0.1,
        
        // G channel: radar background & sweep
        (x < 0.9 ? 
            0.25 // background
            + 0.1 * (s(0.67) + s(0.43) + s(0.2) + k.x + k.y) // grid
            + d * d // detector (sweep fade)
            + max(0.4 - abs(y * (x * 50.0 + 0.3) - 0.4), 0.0) // front of ray
        : 0.0)
        // Outer border (added to Green to make a green border, or we can make it white)
        + max(0.0, 1.0 - abs(l(0.0,0.0) - 0.9) * 50.0),
        
        // B channel: mostly 0.1, but border can be whitish
        0.1 + max(0.0, 1.0 - abs(l(0.0,0.0) - 0.9) * 50.0),
        
        // Alpha: discard pixels outside the border entirely for clean UI
        1.0
    );

    // Alpha masking: make it circular and transparent outside
    if (x > 0.95) {
        discard;
    }

    fragColor = O;
}
