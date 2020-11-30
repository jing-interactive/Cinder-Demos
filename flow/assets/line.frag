#define PI 3.14159265359

// Plot a line on X using a value between 0.0-1.0
float plotOnX(vec2 st, float pct, float k){
  return  smoothstep( pct-k, pct, st.x) -
          smoothstep( pct, pct+k, st.x);
}

// Plot a line on Y using a value between 0.0-1.0
float plotOnY(vec2 st, float pct, float k){
  return  smoothstep( pct-k, pct, st.y) -
          smoothstep( pct, pct+k, st.y);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 st = fragCoord/iResolution.xy;
    st -= 0.5; // normalize to (-0.5, 0.5)
    st *= 2; // scale to (-1, 1)
    st.x *= iResolution.x/iResolution.y;

    float cut = 0.01; //iMouse.y * 100 / iResolution.x;
    vec3 x_axis = plotOnY(st, 0, cut)*vec3(0.5);
    vec3 y_axis = plotOnX(st, 0, cut)*vec3(0.5);
    // float fx = step(0.5, st.x); // y = f(x)
    // float fx = smoothstep(0.3,0.5,st.x) - smoothstep(0.5,0.7,st.x);
    // float fx = 1.0 - pow(abs(st.x), 3.5);
    float fx = sin(st.x * 13 + iTime * 7) / 4 
            + sin(st.x * 5 + iTime * 9) / 6
            + sin(st.x * 7 + iTime * 8) / 3
            + sin(st.x * 3 + iTime * 6) / 2
                ;
    float v = plotOnY(st, fx, 0.3);
    vec3 color_fx = v*vec3(v*0.5, v*0.9, v*0.9);

    fragColor = vec4(x_axis + y_axis + color_fx,1.0);
}