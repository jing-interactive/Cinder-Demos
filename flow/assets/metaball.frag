// https://www.shadertoy.com/view/tsXGDM

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;
    uv -= 0.5;
    uv.x *= iResolution.x/iResolution.y;
    
    // dein circle position and radius
    vec3 cercle1 = vec3(-.2, .01, .07);
    vec3 cercle2 = vec3(.2, -.1, .1);
    vec3 cercle3 = vec3(0., 0., .2);
    vec3 cercle4 = vec3(-0.3, -0.3, .3);
    vec3 cercle5 = vec3(0., 0., .07);

    // animate circle pos and radius
    cercle1.x += sin( (iTime+0.)/4. + sin(iTime +12.) ) / 4.;
    cercle1.y += cos( iTime +cos(iTime*2.) ) /4.;
    
    cercle2.x += cos( iTime/4. + 150.) /2. - sin(iTime/4.	) - .2;
    cercle2.y += (sin( iTime  + 10.) /4.) - cos(iTime)/4.;
    
    cercle3.z += sin((iTime-0.)/4.)/16.;
    cercle3.x += sin(iTime/2.)*.6;
    cercle3.y += sin(iTime*2.)*.10;
      
    cercle4.z = .21 + sin(iTime/4.1)/16.;
    cercle4.x += sin(iTime/4.)/2.;
    cercle4.y += sin((iTime+0.)/16.)/2.;
    
    cercle5.x += sin(iTime/4.)/1.3;
    cercle5.y += sin(iTime/2.)/3.;
    
    // get distance from circle center to uv
    float d1 = distance( cercle1.xy, uv);
    float d2 = distance( cercle2.xy, uv);
   	float d3 = distance( cercle3.xy, uv); 
    float d4 = distance( cercle4.xy, uv); 
    float d5 = distance( cercle5.xy, uv); 
   
    // create a gradiant from circle center to radius
    float c1 = smoothstep( cercle1.z+.2, cercle1.z-0.2, d1 );
    float c2 = smoothstep( cercle2.z+.2, cercle2.z-0.2, d2 );
    float c3 = smoothstep( cercle3.z+.2, cercle3.z-0.2, d3 );
    float c4 = smoothstep( cercle4.z+.2, cercle4.z-0.2, d4 );
    float c5 = smoothstep( cercle5.z+.2, cercle5.z-0.2, d5 );
        
    // sum 
    float c = (c1+c2+c3+c4+c5)*1.5;
        
    // treshold, comment it to see blur
    c = (c > .4 ? 1. : .2);
    
    // color
    float s = (sin(iTime) + 1.) * .5;
    vec3 col = vec3((uv.x+.5) * c, (uv.y+.5) * c, s/2.+.4);
    
    // Output to screen
    fragColor = vec4(col,1.0);
}