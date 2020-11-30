// https://www.shadertoy.com/view/Wsf3Rj
// Companion shader for Raymarching Workshop run at Electric Square

// Workshop site: https://github.com/ajweeks/RaymarchingWorkshop

// MIT license


// This shader provides a playground for designing Signed Distance Functions (SDFs) in
// 2D. Your goal is to fill in the sdf() function below and generate an image on the left
// which is red inside your scene surfaces and green outside. The surface itself has a
// white line.




// SDF utility library functions
float opU(float d1, float d2);
float sdCircle(in vec2 p, in vec2 pos, float radius);
float sdBox(in vec2 p, in vec2 pos, in vec2 size);
float sminCubic(float a, float b, float k);
float opBlend(float d1, float d2);

// noise
// http://shadertoy.wikia.com/wiki/Noise

float hash( float n )
{
    return fract(sin(n)*43758.5453);
}

vec2 hash2( vec2 p ) { p=vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))); return fract(sin(p)*18.5453); }


// --- SDF - TODO! --------------------------------------------------------------

float sdf(vec2 p)
{
    float d = 10.0;
    
    for (int j=0;j<10;j++)
    {
        for (int i=0;i<10;i++)
        {
            vec2 o = hash2(vec2(float(i*0.001+iTime*0.00001), float(j*0.002+iTime*0.00001)));
            o -= vec2(0.5, 0.5);
            o *= 2;
            d = min(d, sdCircle(p, o, hash(o.x*0.0000001) * 0.5));
        }
    }
    
    return d;
}

// ------------------------------------------------------------------------------


// useful functions defined below
vec2 screenToWorld(vec2 screen);
vec3 palxette(in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d);
vec3 shade(float sd);

// compute pixel colour
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // project screen coordinate into world
	vec2 p = screenToWorld(fragCoord);
    
    // signed distance for scene
    float sd = sdf(p);
    
    // compute signed distance to a colour
    vec3 col = shade(sd);
    
    fragColor = vec4(col, 1.0);
}


// --- SDF utility library

float sdCircle(in vec2 p, in vec2 pos, float radius)
{
    return length(p-pos)-radius;
}

float sdBox(in vec2 p, in vec2 pos, in vec2 size)
{
    vec2 d = abs(p-pos)-size;
    return min(0.0, max(d.x, d.y))+length(max(d,0.0));
}

// polynomial smooth min (k = 0.1);
float sminCubic(float a, float b, float k)
{
    float h = max(k-abs(a-b), 0.0);
    return min(a, b) - h*h*h/(6.0*k*k);
}

float opU(float d1, float d2)
{
    return min(d1, d2);
}

float opBlend(float d1, float d2)
{
    float k = 0.2;
    return sminCubic(d1, d2, k);
}



// --- Misc functions


// https://www.shadertoy.com/view/ll2GD3
vec3 palette(in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d)
{
    t = clamp(t, 0., 1.);
    return a + b*cos(6.28318*(c*t+d));
}

vec2 screenToWorld(vec2 screen)
{
    vec2 result = 2.0 * (screen/iResolution.xy - 0.5);
    result.x *= iResolution.x/iResolution.y;
    return result;
}

vec3 shade(float sd)
{
    float maxDist = 2.0;
    vec3 palCol = palette(clamp(0.5-sd*0.4, -maxDist,maxDist), 
                      vec3(0.3,0.3,0.0),vec3(0.8,0.8,0.1),vec3(0.9,0.7,0.0),vec3(0.3,0.9,0.8));

    vec3 col = palCol;
    
    // Darken around surface
	col = mix(col, col*1.0-exp(-10.0*abs(sd)), 0.4);
	// repeating lines
    col *= 0.8 + 0.2*cos(150.0*sd);
    // White outline at surface
    col = mix(col, vec3(1.0), 1.0-smoothstep(0.0,0.01,abs(sd)));
    
    return col;
}
