#define _SnowflakeAmount 1000	// Number of snowflakes
#define _BlizardFactor 0		// Fury of the storm !

vec2 uv;

float rnd(float x)
{
    return fract(sin(dot(vec2(x+47.49,38.2467/(x+2.3)), vec2(12.9898, 78.233)))* (43758.5453));
}

float drawCircle(vec2 center, float radius)
{
    return 1.0 - smoothstep(0.0, radius, length(uv - center));
}

vec2 getCenter(float j, float time)
{
    vec2 center;
#if 0
    float speed = 0.3 + cos(j) * 0.1;//0.3+rnd(cos(j))*(0.7+0.5*cos(j/(float(_SnowflakeAmount)*0.25)));
    center = vec2(
        uv.y*_BlizardFactor+rnd(j)+0.1*cos(time+sin(j)), 
        mod(sin(j)-speed*(time*1.5*(0.1+_BlizardFactor)), 0.65)
    );
#else
    center.x = rnd(j * 2 + time * 0.0001) - 0.5;
    center.y = rnd(j + time * 0.0001) - 0.5;
    // center.x = sin( (j + iTime+0.)/4. + sin(iTime +12.) ) / 4.;
    // center.y = cos( iTime +cos(j + iTime*2.) ) /4.;    
#endif
    return center;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    uv = fragCoord/iResolution.xy;
#if 1
    uv -= 0.5;
    // uv.x *= iResolution.x/iResolution.y;
#endif    
    // fragColor = vec4(0.808, 0.89, 0.918, 1.0);
    float j;
    
    // vec4 bg = texture(iChannel0, uv);
    // fragColor = bg;

    for(int i=0; i<_SnowflakeAmount; i++)
    {
        j = float(i);
        vec2 org_uv = getCenter(j, 0);
        vec4 org_color = texture(iChannel0, org_uv);
        vec2 center = getCenter(j, iTime);
        fragColor += vec4(drawCircle(center, 0.01)) * org_color;
    }
}