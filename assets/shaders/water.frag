#version 330 core

in vec4 vs_color;
in vec2 vs_tex_coord;
in vec3 vs_world; // ray direction

out vec4 frag_color;

uniform float u_time;
uniform vec3 camera_position;

#define AA 1.0
#define STEPS 120.0
#define MDIST 250.0
#define pi 3.1415926535
#define rot(a) mat2(cos(a),sin(a),-sin(a),cos(a))
#define sat(a) clamp(a,0.0,1.0)

#define ITERS_TRACE 9
#define ITERS_NORM 20

#define HOR_SCALE 1.1
#define OCC_SPEED 0.4
#define DX_DET 0.65

#define FREQ 0.6
#define HEIGHT_DIV 2.5
#define WEIGHT_SCL 0.8
#define FREQ_SCL 1.2
#define TIME_SCL 1.095
#define WAV_ROT 1.21
#define DRAG 0.9
#define SCRL_SPEED 0.4
vec2 scrollDir = vec2(1,1);

// ===== CLOUD / SKY PARAMETERS =====
#define DAY_NIGHT_SPEED 0.05
#define SKY_TIME (u_time * DAY_NIGHT_SPEED)

// Smooth day/night cycle direction
#define SUN_DIR         normalize(vec3(0, sin(SKY_TIME), -cos(SKY_TIME)))
#define MOON_DIR        normalize(vec3(0, -sin(SKY_TIME), cos(SKY_TIME)))

// Dynamic sun and moon colors based on height
vec3 get_sun_color() {
    float h = max(SUN_DIR.y, 0.0);
    return mix(vec3(1.0, 0.4, 0.2), vec3(1.0, 0.9, 0.8), h);
}
vec3 get_moon_color() {
    return vec3(0.4, 0.5, 0.7); // Cool blue-ish moon
}

float hash(float n) { return fract(sin(n)*753.5453123); }
float noise(vec3 x) {
    vec3 p = floor(x); vec3 f = fract(x); f = f*f*(3.0 - 2.0*f);
    float n = p.x + p.y*157.0 + 113.0*p.z;
    return mix(mix(mix(hash(n+0.0), hash(n+1.0), f.x), mix(hash(n+157.0), hash(n+158.0), f.x), f.y),
               mix(mix(hash(n+113.0), hash(n+114.0), f.x), mix(hash(n+270.0), hash(n+271.0), f.x), f.y), f.z);
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
    if (moon_amount > 0.999) { 
        vec2 moon_uv = rd.xz; 
        float crater_noise = noise(rd * 150.0) * 0.3 + 0.7; 
        sky += moon_col * 3.0 * crater_noise * smoothstep(-0.1, 0.1, MOON_DIR.y);
    } else {
        sky += moon_col * min(moon_bloom, 1.0) * smoothstep(-0.1, 0.1, MOON_DIR.y);
    }

    return sky;
}

// Simplified version for water reflections without clouds
vec3 sky_eval(vec3 rd) {
    vec3 sky_base = render_sky_color(rd);
    return sky_base;
}

// ===== WATER MAP =====
vec2 wavedx(vec2 wavPos, int iters, float t){
    vec2 dx = vec2(0);
    vec2 wavDir = vec2(1,0);
    float wavWeight = 1.0; 
    wavPos+= t*SCRL_SPEED*scrollDir;
    wavPos*= HOR_SCALE;
    float wavFreq = FREQ;
    float wavTime = OCC_SPEED*t;
    for(int i=0;i<iters;i++){
        wavDir*=rot(WAV_ROT);
        float x = dot(wavDir,wavPos)*wavFreq+wavTime; 
        float result = exp(sin(x)-1.)*cos(x);
        result*=wavWeight;
        dx+= result*wavDir/pow(wavWeight,DX_DET); 
        wavFreq*= FREQ_SCL; 
        wavTime*= TIME_SCL;
        wavPos-= wavDir*result*DRAG; 
        wavWeight*= WEIGHT_SCL;
    }
    float wavSum = -(pow(WEIGHT_SCL,float(iters))-1.)*HEIGHT_DIV; 
    return dx/pow(wavSum,1.-DX_DET);
}

float wave(vec2 wavPos, int iters, float t){
    float wav = 0.0;
    vec2 wavDir = vec2(1,0);
    float wavWeight = 1.0;
    wavPos+= t*SCRL_SPEED*scrollDir;
    wavPos*= HOR_SCALE; 
    float wavFreq = FREQ;
    float wavTime = OCC_SPEED*t;
    for(int i=0;i<iters;i++){
        wavDir*=rot(WAV_ROT);
        float x = dot(wavDir,wavPos)*wavFreq+wavTime;
        float wave = exp(sin(x)-1.0)*wavWeight;
        wav+= wave;
        wavFreq*= FREQ_SCL;
        wavTime*= TIME_SCL;
        wavPos-= wavDir*wave*DRAG*cos(x);
        wavWeight*= WEIGHT_SCL;
    }
    float wavSum = -(pow(WEIGHT_SCL,float(iters))-1.)*HEIGHT_DIV; 
    return wav/wavSum;
}
vec3 norm(vec3 p){
    vec2 wav = -wavedx(p.xz, ITERS_NORM, u_time);
    return normalize(vec3(wav.x,1.0,wav.y));
}
float map(vec3 p){
    p.y-= wave(p.xz,ITERS_TRACE,u_time);
    return p.y;
}

void main() {
    float iTime = u_time;
    // We already have the ray direction directly since it's mapped to a sphere!
    vec3 rd = normalize(vs_world);
    vec3 ro = camera_position;

    vec3 col = vec3(0);
    
    float dO = 0.;
    bool hit = false;
    float d = 0.;
    vec3 p = ro;

    float tPln = -(ro.y - 0.0) / rd.y; 
    
    // Only trace if pointing downwards towards the plane!
    if(tPln>0.){
        dO += tPln;
        for(float i = 0.; i<STEPS; i++){
            p = ro+rd*dO;
            d = map(p);
            dO+=d;
            if(abs(d)<0.005||i>STEPS-2.0){
                hit = true;
                break;
            }
            if(dO>MDIST){
                dO = MDIST;
                break;
            }
        }
    }
    
    vec3 skyrd = sky_eval(rd);
    
    if(hit){
        vec3 n = norm(p);
        vec3 rfl = reflect(rd,n);
        rfl.y = abs(rfl.y);
        vec3 rf = refract(rd,n,1./1.33); 
        
        float fres = clamp((pow(1. - max(0.0, dot(-n, rd)), 5.0)),0.0,1.0);

        vec3 sunDir = SUN_DIR;
               
        col += sky_eval(rfl)*fres*0.9;
        
        float subRefract = pow(max(0.0, dot(rf,sunDir)),35.0);
        vec3 sss_color = vec3(0.1, 0.6, 0.8); 
        col += sss_color * subRefract * 2.5;
        
        vec3 deep_water_color = vec3(0.0, 0.15, 0.3); 
        vec3 waterCol = deep_water_color * (0.8*pow(min(p.y*0.7+0.9,1.8),4.)*length(skyrd));
        col += waterCol*0.17;
        
        col = mix(col,skyrd, smoothstep(0.0, 1.0, dO/MDIST)); 
        
        col = sat(col);
        col = pow(col,vec3(0.87)); 
        
        frag_color = vec4(col,1.0);
    } else {
        // Did not hit water surface, discard and let the sky show through!
        discard;
    }
}
