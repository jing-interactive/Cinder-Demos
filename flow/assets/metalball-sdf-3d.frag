// https://www.shadertoy.com/view/Xds3zN
// The MIT License
// Copyright © 2013 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
    
#define AA 1   // make this 1 is your machine is too slow

#include "common/iq_sdf.glsl"
#include "common/hg_sdf.glsl"

//------------------------------------------------------------------

vec2 map( vec3 p )
{
    float ground = sdPlane(p);
    float m1 = pMod1(p.x, 2.0);
    float m2 = pMod1(p.z, 2.0);
    p.y -= 0;
    float ball = sdSphere(p, 0.6);
    float d = min(ground, ball);
    // res = opU( res, vec2( sdBox(       p-vec3( 1.0,0.25, 0.0), vec3(0.25) ), 3.0 ) );
    // res = opU( res, vec2( sdRoundBox(  p-vec3( 1.0,0.25, 1.0), vec3(0.15), 0.1 ), 41.0 ) );
    // res = opU( res, vec2( sdTorus(     p-vec3( 0.0,0.25, 1.0), vec2(0.20,0.05) ), 25.0 ) );
    // res = opU( res, vec2( sdCapsule(   p-vec3( 1.0,0.,-2.0),vec3(-0.1,0.1,-0.1), vec3(0.2,0.4,0.2), 0.1  ), 31.9 ) );
    // res = opU( res, vec2( sdTriPrism(  p-vec3(-1.0,0.25,-1.0), vec2(0.25,0.05) ),43.5 ) );
    // res = opU( res, vec2( sdCylinder(  p-vec3( 1.0,0.30,-1.0), vec2(0.1,0.2) ), 8.0 ) );
    // res = opU( res, vec2( sdCone(      p-vec3( 0.0,0.50,-1.0), vec3(0.8,0.6,0.3) ), 55.0 ) );
    // res = opU( res, vec2( sdTorus82(   p-vec3( 0.0,0.25, 2.0), vec2(0.20,0.05) ),50.0 ) );
    // res = opU( res, vec2( sdTorus88(   p-vec3(-1.0,0.25, 2.0), vec2(0.20,0.05) ),43.0 ) );
    // res = opU( res, vec2( sdCylinder6( p-vec3( 1.0,0.30, 2.0), vec2(0.1,0.2) ), 12.0 ) );
    // res = opU( res, vec2( sdHexPrism(  p-vec3(-1.0,0.20, 1.0), vec2(0.25,0.05) ),17.0 ) );
    // res = opU( res, vec2( sdPryamid4(  p-vec3(-1.0,0.15,-2.0), vec3(0.8,0.6,0.25) ),37.0 ) );
    // res = opU( res, vec2( opS( sdRoundBox(  p-vec3(-2.0,0.2, 1.0), vec3(0.15),0.05),
    //                            sdSphere(    p-vec3(-2.0,0.2, 1.0), 0.25)), 13.0 ) );
    // res = opU( res, vec2( opS( sdTorus82(  p-vec3(-2.0,0.2, 0.0), vec2(0.20,0.1)),
    //                            sdCylinder(  opRep( vec3(atan(p.x+2.0,p.z)/6.2831, p.y, 0.02+0.5*length(p-vec3(-2.0,0.2, 0.0))), vec3(0.05,1.0,0.05)), vec2(0.02,0.6))), 51.0 ) );
    // res = opU( res, vec2( 0.5*sdSphere(    p-vec3(-2.0,0.25,-1.0), 0.2 ) + 0.03*sin(50.0*p.x)*sin(50.0*p.y)*sin(50.0*p.z), 65.0 ) );
    // res = opU( res, vec2( 0.5*sdTorus( opTwist(p-vec3(-2.0,0.25, 2.0)),vec2(0.20,0.05)), 46.7 ) );
    // res = opU( res, vec2( sdCappedCone( p-vec3( 0.0,0.35,-2.0), 0.15, 0.2, 0.1 ), 13.67 ) );
    // res = opU( res, vec2( sdEllipsoid( p-vec3(-1.0,0.3,0.0), vec3(0.2, 0.25, 0.05) ), 43.17 ) );
    // res = opU( res, vec2( sdRoundCone( p-vec3(-2.0,0.2,-2.0), 0.2, 0.1, 0.3 ), 23.56 ) );
    // res = opU( res, vec2( sdCylinder( p-vec3(2.0,0.2,-1.0), vec3(0.1,-0.1,0.0), vec3(-0.1,0.3,0.1), 0.08), 31.2 ) );
    // res = opU( res, vec2( sdRoundCone( p-vec3(2.0,0.2,-2.0), vec3(0.1,0.0,0.0), vec3(-0.1,0.3,0.1), 0.15, 0.05), 51.7 ) );
        
    return vec2(d, abs(m2));
}

vec3 render( vec3 ro, vec3 rd )
{ 
    vec3 col = vec3(0.7, 0.9, 1.0) + rd.y;
    vec2 res = castRay(ro,rd);
    float t = res.x;
    float m = res.y;
    if( m>-1.0 )
    {
        vec3 p = ro + t*rd;
        vec3 nor = calcNormal( p );
        vec3 ref = reflect( rd, nor );
        
        // material
        col = vec3(m);
        // if( m<10 )
        // {
        //     float f = checkersGradBox( 5.0*p.xz );
        //     col = 0.3 + f*vec3(0.1);
        // }
        // lighitng
        float occ = calcAO( p, nor );
        vec3  lig = normalize( vec3(-0.4, 0.7, -0.6) );
        vec3  hal = normalize( lig-rd );
        float amb = clamp( 0.5+0.5*nor.y, 0.0, 1.0 );
        float dif = clamp( dot( nor, lig ), 0.0, 1.0 );
        float bac = clamp( dot( nor, normalize(vec3(-lig.x,0.0,-lig.z))), 0.0, 1.0 )*clamp( 1.0-p.y,0.0,1.0);
        float dom = smoothstep( -0.2, 0.2, ref.y );
        float fre = pow( clamp(1.0+dot(nor,rd),0.0,1.0), 2.0 );
        
        dif *= calcSoftshadow( p, lig, 0.02, 2.5 );
        dom *= calcSoftshadow( p, ref, 0.02, 2.5 );

        float spe = pow( clamp( dot( nor, hal ), 0.0, 1.0 ),16.0)*
                    dif *
                    (0.04 + 0.96*pow( clamp(1.0+dot(hal,rd),0.0,1.0), 5.0 ));

        vec3 lin = vec3(0.0);
        lin += 1.30*dif*vec3(1.00,0.80,0.55);
        lin += 0.30*amb*vec3(0.40,0.60,1.00)*occ;
        lin += 0.40*dom*vec3(0.40,0.60,1.00)*occ;
        lin += 0.50*bac*vec3(0.25,0.25,0.25)*occ;
        lin += 0.25*fre*vec3(1.00,1.00,1.00)*occ;
        col = col*lin;
        col += 9.00*spe*vec3(1.00,0.90,0.70);

        col = mix( col, vec3(0.8,0.9,1.0), 1.0-exp( -0.0002*t*t*t ) );
    }

    return vec3( clamp(col,0.0,1.0) );
}
