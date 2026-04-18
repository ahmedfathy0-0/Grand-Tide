#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} fs_in;

out vec4 fragColor;

uniform float time;
uniform float hp_percent;
uniform vec3 water_color;

#define  SIZE  0.5
#define WATER_DEEP 0.6
#define PI 3.1415927
#define Deg2Radius PI/180.

float Rand(float x)
{
    return fract(sin(x*866353.13)*613.73);
}

mat2 Rotate2D(float deg){
    deg = deg * Deg2Radius;
	return mat2(cos(deg),sin(deg),-sin(deg),cos(deg));
}
vec2 Within(vec2 uv, vec4 rect) {
	return (uv-rect.xy)/(rect.zw-rect.xy);
}
float Remap(float a,float b,float c,float d,float val){
	return (val-a)/(b-a) * (d-c) + c;
}

float Circle(vec2 uv,vec2 center,float size,float blur){
	uv = uv - center;
	uv /= size;
	float len = length(uv);
	return smoothstep(1.,1.-blur,len);
}

float PureCircle(vec2 uv,vec2 center,float size,float blur,float powVal){
	uv = uv - center;
	uv /= size;
	float len = 1.-length(uv);
    float val = clamp(Remap(0.,blur,0.,1.,len),0.,1.);
    return pow(val,powVal);
}
float Ellipse(vec2 uv,vec2 center,vec2 size,float blur){
	uv = uv - center;
	uv /= size;
	float len = length(uv);
	return smoothstep(1.,1.-blur,len);
}

float Torus2D(vec2 uv,vec2 center,vec2 size,float blur){
	uv = uv - center;
	float len = length(uv);
    if(len<size.y || len >size.x)
        return 0.;
    float radio = (len-size.y)/(size.x-size.y);
    float val = 1.-abs((radio-0.5)*2.);
	return pow(val,blur);
}

vec3 DrawFrame(vec2 uv){
    vec3 frameCol = vec3(0.9,0.75,0.6);
    float frameMask = Torus2D(uv,vec2(0.,0.),vec2(SIZE * 1.1,SIZE),0.2);
    return frameMask * frameCol;
}

vec3 DrawHightLight(vec2 uv){
    vec3 hlCol = vec3(0.95,0.95,0.95);
    float upMask = Ellipse(uv,vec2(0.,0.8)*SIZE,vec2(0.9,0.7)*SIZE,0.6)*0.9;
    upMask = upMask * Circle(uv,vec2(0.,0.)*SIZE,SIZE*0.95,0.02) ;
    upMask = upMask * Circle(uv,vec2(0.,-0.9)*SIZE,SIZE*1.1,-0.8) ;
    uv = Rotate2D(30.) * uv;
    float btMask =1.;
    btMask *=  Circle(uv,vec2(0.,0.)*SIZE,SIZE*0.95,0.02);
    float scale = 0.9;
    btMask *= 1.- Circle(uv,vec2(0.,-0.17+scale)*SIZE,SIZE*(1.+scale),0.2) ;
    return  (upMask + btMask) * hlCol;
}

float GetWaveHeight(vec2 uv){
    uv = Rotate2D(-30.)*uv;
	float wave =  0.12*sin(-2.*uv.x+time*4.); 
	uv = Rotate2D(-50.)*uv;
	wave +=  0.05*sin(-2.*uv.x+time*4.); 
	return wave;
}

float RayMarchWater(vec3 camera, vec3 dir,float startT,float maxT){
    vec3 pos = camera + dir * startT;
    float t = startT;
    for(int i=0;i<200;i++){
        if(t > maxT){
        	return -1.;
        }
        float h = GetWaveHeight(pos.xz) * WATER_DEEP;
        if(h + 0.01 > pos.y ) {
            return t;
        }
        t += pos.y - h; 
        pos = camera + dir * t;
    }
    return -1.0;
}

vec4 SimpleWave3D(vec2 uv,vec3 col){
	vec3 camPos =vec3(0.23,0.115,-2.28);
    vec3 targetPos = vec3(0.);
    
    vec3 f = normalize(targetPos-camPos);
    vec3 r = cross(vec3(0., 1., 0.), f);
    vec3 u = cross(f, r);
    
    vec3 ray = normalize(uv.x*r+uv.y*u+1.0*f);
    
	float startT = 0.1;
    float maxT = 20.;
	float dist = RayMarchWater(camPos, ray,startT,maxT);
    vec3 pos = camPos + ray * dist;
    float circleSize = 2.;
    if(dist < 0.){
    	return vec4(0.,0.,0.,0.);
    }
    vec2 offsetPos = pos.xz;
    if(length(offsetPos)>circleSize){
    	return vec4(0.,0.,0.,0.);
    }
    float colVal = 1.-((pos.z+0.)/circleSize +1.0) *.5;
	return vec4(col*smoothstep(0.,1.4,colVal),1.);
}

float SmoothCircle(vec2 uv,vec2 offset,float size){
    uv -= offset;
    uv/=size;
    float temp = clamp(1.-length(uv),0.,1.);
    return smoothstep(0.,1.,temp);
}

float DrawBubble(vec2 uv,vec2 offset,float size){
    uv = (uv - offset)/size;
    float val = length(uv);
    val = smoothstep(0.5,2.,val)*step(val,1.);
    val +=SmoothCircle(uv,vec2(-0.2,0.3),0.6)*0.4;
    val +=SmoothCircle(uv,vec2(0.4,-0.5),0.2)*0.2;
	return val; 
}

float DrawBubbles(vec2 uv){
	uv = Within(uv, vec4(-SIZE,-SIZE,SIZE,SIZE));
    uv.x-=0.5;
    float val = 0.;
    const float count = 2.;
    const float maxVY = 0.1;
    const float ay = -.3;
    const  float ax = -.5;
    const  float maxDeg = 80.;
    const float loopT = maxVY/ay + (1.- 0.5*maxVY*maxVY/ay)/maxVY;
    const  float num = loopT*count;
    for(float i=1.;i<num;i++){
    	float size = 0.02*Rand(i*451.31)+0.02;
        float t = mod(time + Rand(i)*loopT,loopT);
        float deg = (Rand(i*1354.54)*maxDeg +(90.-maxDeg*0.5))*Deg2Radius;
        vec2 vel = vec2(cos(deg),sin(deg));
        float ty = max((vel.y*0.3 - maxVY),0.)/ay;
        float yt = clamp(t,0.,ty);
		float y = max(0.,abs(vel.y)*yt + 0.5*ay*yt*yt) + max(0.,t-ty)*maxVY;
        
        float tx = abs(vel.x/ax);
        t = min(tx,t);
        float xOffset = abs(vel.x)*t+0.5*ax*t*t + sin(time*(0.5+Rand(i)*2.)+Rand(i)*2.*PI)*0.03;
        float x = sign(vel.x)*xOffset;
        vec2 offset = vec2(x,y);
    	val += DrawBubble(uv,offset,size*0.5);
    }
	return val;
}

void main()
{
	vec2 uv = fs_in.tex_coord * 2.0 - 1.0; 
    uv.y = -uv.y; // Flip Y-axis to account for orthographic projection (top-left origin)
    
    float len = length(uv);
    if(len > SIZE * 1.12) {
        discard;
    }
    
    vec3 col = vec3(0.,0.,0.);
    col += DrawFrame(uv);
    
    float hpPerMask = step(0.,(hp_percent *2. -1.)*SIZE - uv.y);
  	float bgMask = 0.;
    bgMask += PureCircle(uv,vec2(0.,0.),SIZE*1.1,.9,0.9);
 	bgMask += Circle(uv,vec2(0.,0.),SIZE,.6)*0.2;
    col += bgMask * water_color *hpPerMask ;
    
    float waterMask = step(length(uv),SIZE);
    float offset = hp_percent -0.5+0.01;
    float wavePicSize = 0.8*SIZE;
    vec2 remapUV = Within(uv,vec4(0.,offset,wavePicSize,offset+wavePicSize-0.2));
    vec4 wave = SimpleWave3D(remapUV,water_color);
    col = mix(col,wave.xyz*bgMask,wave.w*waterMask);
	
    float bubbleMask = smoothstep(0.,0.1,(hp_percent *2. -1.2)*SIZE - uv.y);
    col+= DrawBubbles(uv)*vec3(1.)* bubbleMask*waterMask;
    col += DrawHightLight(uv*1.);
    
    fragColor = vec4(col, 1.0);
}
