#include "common/print.glsl"

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec3 vColour = vec3(0.0);

    // Plot Mouse Pos
    float fDistToPointB = length( vec2(iMouse.x, iMouse.y) - fragCoord.xy) - 8.0;
    vColour = mix(vColour, vec3(0.0, 1.0, 0.0), (1.0 - clamp(fDistToPointB, 0.0, 1.0)));

    // Print mouse coord
    float f1 = EasyPrintValue(iMouse.xy, fragCoord, iMouse.x);
    vColour = mix( vColour, vec3(0.0, 1.0, 1.0), f1);
    float f2 = EasyPrintValue(iMouse.xy + vec2(60, 0), fragCoord, iMouse.y);
    vColour = mix( vColour, vec3(1.0, 1.0, 0.0), f2);

    fragColor = vec4(vColour,1.0);
}
