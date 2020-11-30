// SnowScreen (superposition of blobs in displaced-grid voronoi-cells) by Jakob Thomsen
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

#include "common/print.glsl"

#define pi 3.1415926
#define SPEED 2

// iq's hash function from https://www.shadertoy.com/view/MslGD8
vec2 hash( vec2 p ) { p=vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))); return fract(sin(p)*18.5453); }

// hash_seed = floor(v)
vec2 getPos(vec2 hash_seed, float t)
{
    return cos(2. * pi * (t*0.1 + hash(hash_seed)) + vec2(0,0));
}

// http://iquilezles.org/www/articles/voronoilines/voronoilines.htm
float simplegridnoise(vec2 v, float t)
{
    vec2 fl = floor(v);
    vec2 fr = fract(v);
    float mindist = 1e9;
    for(int y = -1; y <= 1; y++)
        for(int x = -1; x <= 1; x++)
        {
            vec2 offset = vec2(x, y);
            vec2 pos = getPos(fl + offset, t);
            mindist = min(mindist, length(pos+offset -fr));
        }
    
    return mindist;
}

float blobnoise(vec2 v, float s, float t)
{
    return pow(.5 + .5 * cos(pi * clamp(simplegridnoise(v, t)*2., 0., 1.)), s);
}

float fractalblobnoise(vec2 v, float s, float t)
{
    float val = 0.;
    const float n = 4.;
    for (float i = 0.; i < n; i++)
        // val += 1.0 / (i + 1.0) * blobnoise((i + 1.0) * v + vec2(0.0, t * 1.0), s, t);
        val += blobnoise(exp2(i) * v - vec2(0, t*SPEED), s, t);

    return val;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec3 vColour = vec3(0.0);
#if 0
    float t = TEST_VEC4.x;
#else
    float t = iTime;
#endif

    vec2 r = vec2(4, 0.2 * iResolution.y / iResolution.x);
	vec2 uv = fragCoord.xy / iResolution.xy;
    // uv = iMouse.xy / iResolution.xy;
    float val = fractalblobnoise(r * uv * 30.0, 3.5, t);
    //float val = blobnoise(r * uv * 10.0, 5.0);
    //fragColor = vec4(vec3(val), 1.0);
    // fragColor = mix(texture(iChannel0, uv), vec4(1.0), vec4(val));
// vec2 getPos(vec2 hash_seed, float t)
    vec2 orgUV = getPos((r * vec2(iMouse.x,iMouse.y) * 30.0), 0);
    vColour = texture(iChannel0, orgUV).rgb * vec3(val);

    // Plot Mouse Pos
    float fDistToPointB = length( vec2(iMouse.x, iMouse.y) - fragCoord.xy) - 8.0;
    vColour = mix( vColour, vec3(0.0, 1.0, 1.0), EasyPrintValue(vec2(50, 50), fragCoord, orgUV.x));
    vColour = mix( vColour, vec3(0.0, 1.0, 1.0), EasyPrintValue(vec2(100, 50), fragCoord, orgUV.y));

    fragColor = vec4(vColour,1.0);
}
