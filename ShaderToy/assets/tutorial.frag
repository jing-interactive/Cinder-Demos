/*
HOWTO Get Started With Ray Marching
by Michael Pohoreski aka MysticReddit
Version 0.48, Feb. 2017

= Introduction =

Are you wondering how some of those awesome ShaderToy demos are created?
Want to learn about ray marching but have NO idea how to start?

This is a mini-tutorial on how to get started with ray marching
in the spirit of the famous "NeHe OpenGL Tutorials".
I dub it the PoHo of Ray Marching. :-)

For now we're not going to write a ray marching renderer; instead we'll
simply play around with an existing one to get familiar the techniques
of constructive solid geometry and distance functions to see how they work.

= Steps =

1. First, you'll want to read this introduction:
   Don't worry if it seems complicated and you don't understand everything.
   This is just to get familiar with some concepts.

       http://www.iquilezles.org/www/material/nvscene2008/rwwtt.pdf

2. Next, try out the provided examples!
   I've provided some "lessons" where you can try out small demos.

   Change the # to the lesson you want. (See Table of Contents below)
   Then hit the |> triangle button below the code but above the 'iChannel0' picture.
*/

#define LESSON 1

/*
= Lesson Table of Contents =

    Lesson  0 --- see "Creative time!" note below ---

    // Primitives
ok  Lesson  1 Primitive: Sphere
ok  Lesson  2 Primitive: Two Spheres
ok  Lesson  3 Primitive: Three Spheres
ok  Lesson  4 Primitive: (Signed) Box
ok  Lesson  5 Primitive: (Unsigned) Round Box
ok  Lesson  6 Primitive: Torus
ok  Lesson  7 Primitive: Cone
fix Lesson  8 Primitive: Plane
ok  Lesson  9 Primitive: Pyramid
ok  Lesson 10 Primitive: Cylinder
ok  Lesson 11 Primitive: Capsule
ok  Lesson 12 Composite: Capsule (sin + Box)

    // Constructive Solid Geometry FTW!
ok  Lesson 13 Distance:  Operation: Union (Box + Sphere)
ok  Lesson 14 Distance:  Operation: Subtraction (Box - Sphere)
ok  Lesson 15 Distance:  Operation: Intersection

    // Scale, Rotate, Translate: p' = S*R*T*p
ok  Lesson 16 Domain:    Operation: Translation (Box)
ok  Lesson 17 Domain:    Operation: Rotation (Box)
ok  Lesson 18 Domain:    Operation: Scale
ok  Lesson 19 Domain:    Operation: Matrix: Translation
ok  Lesson 20 Domain:    Operation: Matrix: Translation & Rotation (box)

ok  Lesson 21 Domain:    Operation: Repetition (Spere)
ok  Lesson 22 Domain:    Operation: Repetition (Box -> Cube)

    // Deformations
ok  Lesson 23 Domain:    Deformation: (Cheap) Bend X (Box)
ok  Lesson 24 Domain:    Deformation: (Cheap) Bend Y (Box)
ok  Lesson 25 Domain:    Deformation: (Cheap) Bend Z (Box)

fix Lesson 26 Domain:    Deformation: Twist X (Box)
fix Lesson 27 Domain:    Deformation: Twist Y (Box)
fix Lesson 28 Domain:    Deformation: Twist Z (Box)

ok  Lesson 29 Distance:  Deformation: Displacement (Torus)
ok  Lesson 30 Distance:  Deformation: Blend (Box + Torus)

ok  Lesson 101 Empty Glass
ok  Lesson 102 Crystal Skull

NOTE: Not all the lessons work (yet)!

    Legend:
        ok   means it has been verified working.
        fix  means it is partially working but needs debugging
        BAD  means it is completely broken


= HOW TO (cont.) =

3. Distance Functions

   As you go through the examples you'll want to become familiar with these distance functions:

       http://www.iquilezles.org/www/articles/distfunctions/distfunctions.htm

4. View iq's "Reference" implementation

    "Raymarching - Primitives
    https://www.shadertoy.com/view/Xds3zN


5. Creative time!

   a) Change the lesson to zero via:
      #define LESSON 0

   b) Modify the draw() function
      Create your masterpiece code in the section labeled 'CREATE ME!

Remember:

 *Everyone* was a "noob" at one point!
 *None* of us was born knowing this stuff!

Have fun!


Master Refence:

    http://d.hatena.ne.jp/hanecci/20131005/p1

More references:

    Raymarching - Primitives
    https://www.shadertoy.com/view/Xds3zN

    http://blog.ruslans.com/2015/01/raymarching-christmas-tree.html
    https://www.shadertoy.com/view/XlXGRM

    http://www.hugi.scene.org/online/hugi37/hugi%2037%20-%20coding%20adok%20on%20ray%20casting,%20ray%20tracing,%20ray%20marching%20and%20the%20like.htm

    http://www.pouet.net/topic.php?which=7931&page=1

    A "simple" ray marcher
    Menger Sponge
    https://www.shadertoy.com/view/4sX3Rn
    http://www.iquilezles.org/www/articles/menger/menger.htm]

Note: This code was shamelessly copied and cleaned up from the excellent demo:

    "Crystal Skull"
    https://www.shadertoy.com/view/MsS3WV

Once you start feeling comfortable you can distill shaders down to their bare essence.

    "Empty Glass"
    https://www.shadertoy.com/view/4s2GDV

As an homage I've provided them here as lessons. :-)

Special Thanks:

* Inigo Quilez - for ShaderToy and for sharing his rendering knowledge!

    Can you _please_ fix the typos in distfunctions.htm ? :-)
    If you need a technical editor I'd be more then happy to be one. :-)

* Everyone who posted their cool shaders!

  * "Chocolux"
  * "Road to Ribbon"

"If I have seen further it is by standing on the shoulders of giants." -- Isaac Newton

History:

    0.49 Fix uncomment for AUTO_ROTATE
    0.48 cleanup grammar
    0.47 rename transpose to transposeM4() for matrix shenanigans 
    0.46 Uncomment transpose() for WebGL 2.0 Shadertoy upgrade
    0.45 Clarified compile button
    0.44 Provided instruction on HOW TO test the next lesson
    0.43 Provide second material
    0.42 Cleanup introduction
    0.41 Rot4Y
    0.40 cleanup
    0.38 fixed Lesson 30
    0.37 fixed Lesson 29
*/

#define AUTO_ROTATE     // uncomment to stop auto camera rotation
//#define BACKGROUND_BLUE // uncomment for blue background, else cubemap background
//#define VIEW_ZERO       // uncomment to default OpenGL look down z-axis view
#define VIEW_ISOMETRIC  // Nice isometric camera angle

//#define LOW_Q // uncomment for low quality if your GPU is a potato

#ifdef LOW_Q
    #define MARCHSTEPS 25
#else
    #define MARCHSTEPS 50
    #define AMBIENT_OCCLUSION
    #define DOUBLE_SIDED_TRANSPARENCY
#endif

#define MAX_DIST 10.0

#define SPECULAR
#define REFLECTIONS
#define TRANSPARENCY
#define SHADOWS
#define FOG

//TODO: FIXME: Do Displacement and Blend need flat shading?
#if LESSON == 26 || LESSON == 27
    #undef AMBIENT_OCCLUSION
    #undef DOUBLE_SIDED_TRANSPARENCY
    #undef TRANSPARENCY
#endif

#define DIRECTIONAL_LIGHT
#define DIRECTIONAL_LIGHT_FLARE

#define PI 3.141592654

#define kNt  -1.0 //no trans
#define kTt   1.0 //yes trans
#define kIt   0.0 //inverse trans

const float MATERIAL_1 = 1.0;
const float MATERIAL_2 = 2.0;
/* */ float gMaterial  = MATERIAL_1;

// TODO: Document these structure member fields!
// rd Ray Direction
// rl Ray Length
struct sRay   { vec3 ro ; vec3  rd ; float sd; float rl; };
struct sHit   { vec3 hp ; float hd ; vec3 oid; };
struct sSurf  { vec3 nor; vec3  ref; vec3 tra; };
struct sMat   { vec3 ctc; float frs; float smt; vec2 par; float trs; float fri; };
struct sShade { vec3 dfs; vec3  spc; };
struct sLight { vec3 rd ; vec3  col; };

// __ Matrix functions __ _____________________________________

    // Return 2x2 rotation matrix
    // With vector swizzle/mask can use as a 3x3 xform
    // For y, you need to invert 
    // angle in radians
    // ========================================
    mat2 Rot2(float a ) {
        float c = cos( a );
        float s = sin( a );
        return mat2( c, -s, s, c );
    }

    // http://www.songho.ca/opengl/gl_anglestoaxes.html

    // Return 4x4 rotation X matrix
    // angle in radians
    // ========================================
    mat4 Rot4X(float a ) {
        float c = cos( a );
        float s = sin( a );
        return mat4( 1, 0, 0, 0,
                     0, c,-s, 0,
                     0, s, c, 0,
                     0, 0, 0, 1 );
    }

    // Return 4x4 rotation Y matrix
    // angle in radians
    // ========================================
    mat4 Rot4Y(float a ) {
        float c = cos( a );
        float s = sin( a );
        return mat4( c, 0, s, 0,
                     0, 1, 0, 0,
                    -s, 0, c, 0,
                     0, 0, 0, 1 );
    }

    // Return 4x4 rotation Z matrix
    // angle in radians
    // ========================================
    mat4 Rot4Z(float a ) {
        float c = cos( a );
        float s = sin( a );
        return mat4(
            c,-s, 0, 0,
            s, c, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
         );
    }

    // Translate is simply: p - d
    // opTx will do transpose(m)
    // p' = m*p
    //    = [m0 m1 m2 m3 ][ p.x ]
    //      [m4 m5 m6 m7 ][ p.y ]
    //      [m8 m9 mA mB ][ p.z ]
    //      [mC mD mE mF ][ 1.0 ]
    // ========================================
    mat4 Loc4( vec3 p ) {
        p *= -1.;
        return mat4(
            1,  0,  0,  p.x,
            0,  1,  0,  p.y,
            0,  0,  1,  p.z,
            0,  0,  0,  1
        );
    }


    // if no support for GLSL 1.2+
    //     #version 120
    // ========================================
    mat4 transposeM4(in mat4 m ) {
        vec4 r0 = m[0];
        vec4 r1 = m[1];
        vec4 r2 = m[2];
        vec4 r3 = m[3];

        mat4 t = mat4(
             vec4( r0.x, r1.x, r2.x, r3.x ),
             vec4( r0.y, r1.y, r2.y, r3.y ),
             vec4( r0.z, r1.z, r2.z, r3.z ),
             vec4( r0.w, r1.w, r2.w, r3.w )
        );
        return t;
    }


// __ Smoothing functions _____________________________________

    // Smooth Min
    // http://www.iquilezles.org/www/articles/smin/smin.htm

    // Min Polynomial
    // ========================================
    float sMinP( float a, float b, float k ) {
        float h = clamp( 0.5+0.5*(b-a)/k, 0.0, 1.0 );
        return mix( b, a, h ) - k*h*(1.0-h);
    }

    // Min Exponential
    // ========================================
    float sMinE( float a, float b, float k) {
        float res = exp( -k*a ) + exp( -k*b );
        return -log( res )/k;
    }

    // Min Power
    // ========================================
    float sMin( float a, float b, float k ) {
        a = pow( a, k );
        b = pow( b, k );
        return pow( (a*b) / (a+b), 1.0/k );
    }

// __ Surface Primitives ____________________________

    // Return max component x, y, or z
    // ========================================
    float maxcomp(in vec3 p ) {
        return max(p.x,max(p.y,p.z));
    }

// Signed

    // b.x = Width
    // b.y = Height
    // b.z = Depth
    // Leave r=0 if radius not needed
    // ========================================
    float sdBox(vec3 p, vec3 b, float r) {
        vec3 d = abs(p) - b;
        return min(maxcomp(d),0.0) - r + length(max(d,0.0));
        // Inlined maxcomp
        //return min(max(d.x,max(d.y,d.z)),0.0) - r + length(max(d,0.0));
    }

    // ========================================
    float sdCappedCylinder( vec3 p, vec2 h ) {
        vec2 d = abs(vec2(length(p.xz),p.y)) - h;
        return min(max(d.x,d.y),0.0) + length(max(d,0.0));
    }

    // ========================================
    float sdCapsule( vec3 p, vec3 a, vec3 b, float r ) {
        vec3 pa = p - a, ba = b - a;
        float h = clamp( dot(pa,ba) / dot(ba,ba), 0.0, 1.0 );
        return length( pa - ba*h ) - r;
    }

    // c.x Width
    // c.y Base Radius
    // c.z Depth
    // Note: c must be normalized
    // ========================================
    float sdCone( vec3 p, vec3 c) // TODO: do we need to use 'in' for all primitives?
    {
        // c.x = length
        // c.y = base radius
        //float q = length( p.xy );
        //return dot( c, vec2( q, p.z ) ); // BUG in iq's docs -- laying on side

        float q = length( p.xz );
        return dot( c.xy, vec2( q, p.y ) );

        // Alt. cone formula given in: ???
        //vec2 q = vec2( length( p.xz ), p.y );
        //float d1 = -p.y - c.z;
        //float d2 = max( dot(q,c.xy), p.y );
        //return length(max(vec2(d1,d2),0.0)) + min(max(d1,d2), 0.0);
    }

    // ========================================
    float sdCylinder( vec3 p, vec3 c ) {
        return length(p.xz - c.xy) - c.z;
    }

    // n.xyz = point on plane
    // n.w   = distance to plane
    // Note: N must be normalized!
    // ========================================
    float sdPlane( vec3 p, vec4 n ) {
        return dot( p, n.xyz ) + n.w;
    }

    // 4 sided pyramid
    // h.x = base X
    // h.y = height
    // h.z = base Z (usually same as h.x)
    // ========================================
    float sdPyramid4( vec3 p, vec3 h ) {
        p.xz = abs(p.xz);                   // Symmetrical about XY and ZY
        vec3 n = normalize(h);
        return sdPlane(p, vec4( n, 0.0 ) ); // cut off bottom
    }

    // ========================================
    float sdSphere( vec3 p, float r ) {
        return length(p) - r;
    }

    // ========================================
    float sdSphere2( vec3 p, float r ) {
        return abs(length(p) - r);
    }

    // ========================================
    float sdTorus( vec3 p, vec2 t ) {
        vec2 q = vec2(length(p.xy) - t.x, p.z);
        return length(q) - t.y;
    }

    // TODO: document/derive magic number 0.866025
    // ========================================
    float sdTriPrism( vec3 p, vec2 h ) {
        vec3 q = abs(p);
        return max(q.z-h.y,max(q.x*0.866025+p.y*0.5,-p.y)-h.x*0.5);
    }

// Unsigned

    // Box
    // ========================================
    float udBox( vec3 p, vec3 b ) {
        return length( max( abs(p) - b, 0.0 ) );
    }

    // Round Box
    // ========================================
    float udRoundBox(vec3 p, vec3 b, float r)
    {
        return length(max(abs(p) - b, 0.0))- r;
    }

// __ Distance Operations _____________________________________

// Basic
    // Op Union
    // ========================================
    float opU( float d1, float d2 ) {
        return min( d1, d2 );
    }

    // Op Union
    // ========================================
    vec4 opU2( vec4 d1, vec4 d2 ) {
        return min( d1, d2 );
    }

    // Op Union
    // ========================================
    vec4 opU( vec4 a, vec4 b ) {
        return mix(a, b, step(b.x, a.x));
    }

    // Op Subtraction
    // ========================================
    float opS( float a, float b ) {
        return max( -b, a ); // BUG in iq's docs: -a, b
    }
    // Op Subtraction
    // ========================================
    vec4 opS( vec4 a, vec4 b ) {
        return max( -b, a );
    }

    // Op Intersection
    // ========================================
    float opI( float a, float b ) {
        return max( a, b );
    }

    // Op Intersection
    // ========================================
    vec4 opI( vec4 a, vec4 b ) {
        return max( a, b );
    }

// Advanced
    // ========================================
    float opBlend( float a, float b, float k ) {
        return sMin( a, b, k );
    }

    // a angle
    // ========================================
    float displacement( vec3 p, float a ) {
        return sin(a*p.x)*sin(a*p.y)*sin(a*p.z); // NOTE: Replace with your own!
    }

    // ========================================
    float opDisplace( vec3 p, float d1, float d2 ) {
        return d1 + d2;
    }

    // Op Union Translated
    // ========================================
    vec4 opUt( vec4 a, vec4 b, float fts ){
        vec4 vScaled = vec4(b.x * (fts * 2.0 - 1.0), b.yzw);
        return mix(a, vScaled, step(vScaled.x, a.x) * step(0.0, fts));
    }


// __ Domain Operations _______________________________________

// NOTE: iq originally inlined the primitive inside the Domain operations. :-(
// This implied that you would have needed to provide 
// a primitive with one of the sd*() functions above
// since we can't have a generic pointer to a function!
// However we have moved them back out to the caller
// for clarity and flexibility without general loss of precision.

// Basic

    // Op Repetition
    // ========================================
    vec3 opRep( vec3 p, vec3 spacing ) {
        return mod(p,spacing) - 0.5*spacing;
    }

// Deformations

    // Op Twist X
    // ========================================
    vec3 opTwistX( vec3 p, float angle ) {
        mat2 m = Rot2( angle * p.x );
        return   vec3( m*p.yz, p.x );
    }

    // Op Twist Y
    // ========================================
    vec3 opTwistY( vec3 p, float angle ) {
#if 0 // original
        float c = cos( angle * p.y );
        float s = sin( angle * p.y );
        mat2  m = mat2( c, -s, s, c );
        vec3  q = vec3( m*p.xz, p.y );
        // return primitive(q); // BUG in iq's docs, should be: return q
        return q;
#else // cleaned up
        mat2 m = Rot2( angle * p.y );
        return   vec3( m*p.xz, p.y );
#endif
    }

    // Op Twist Z
    // ========================================
    vec3 opTwistZ( vec3 p, float angle ) {
        mat2 m = Rot2( angle * p.z );
        return   vec3( m*p.xy, p.z );
    }

    // iq's bend X
    // ========================================
    vec3 opCheapBend( vec3 p, float angle ) {
#if 0 // original // broken :-(
        float c = cos( angle * p.y );
        float s = sin( angle * p.y );
        mat2  m = mat2( c, -s, s, c );
        vec3  q = vec3( m*p.xy, p.z ); // BUG in iq's docs, should be: p.yx
#else
        mat2  m = Rot2( angle * p.y );
        vec3  q = vec3( m*p.yx, p.z );
#endif
        return q;
    }

    // Op Cheap Bend X
    // ========================================
    vec3 opBendX( vec3 p, float angle ) {
        mat2 m = Rot2( angle * p.y );
        return   vec3( m*p.yx, p.z );
    }

    // Op Cheap Bend Y
    // ========================================
    vec3 opBendY( vec3 p, float angle ) {
        mat2 m = Rot2( angle * p.z );
        return   vec3( m*p.zy, p.x );
    }

    // Op Cheap Bend Z
    // ========================================
    vec3 opBendZ( vec3 p, float angle ) {
        mat2 m = Rot2( angle * p.x );
        return   vec3( m*p.xz, p.y );
    }

    // d = distance to move
    // ========================================
    vec3 opTrans( vec3 p, vec3 d ) {
        return p - d;
    }

    // Note: m must already be inverted!
    // TODO: invert(m) transpose(m)
    // Op Rotation / Translation
    // ========================================
    vec3 opTx( vec3 p, mat4 m ) {   // BUG in iq's docs, should be q
        return (transposeM4(m)*vec4(p,1.0)).xyz;
    }

    // Op Scale
    // ========================================
    float opScale( vec3 p, float s ) {
        return sdBox( p/s, vec3(1.2,0.2,1.0), 0.01 ) * s; // TODO: FIXME: NOTE: replace with primative sd*()
    }

// The fun starts here!
// ========================================
float draw( vec3 p )
{
    vec3  q = p  ; // save original point
    float d = 0.0; // distance function; default to no intersection

#define BOX_W  1.50
#define BOX_H  0.25
#define BOX_D  0.75
#define RADIUS 0.666

#if LESSON == 1 // Primitive: Sphere
    p = q;
    d = sdSphere( p, 0.5 ); // position, radius
#endif


#if LESSON == 2 // Primitive: Two Spheres
    float r;

    p = q;
    r = 0.5;
    p.x -= r;
    float o1 = sdSphere( p, r );

    p = q;
    r = 0.25;
    p.x += r;
    float o2 = sdSphere( p, r );
    d = opU( o1, o2 );
#endif


#if LESSON == 3 // Primitive: Three Spheres
    p = q;
    d = MAX_DIST;
    for( int i = 0; i < 3; i++ )
    {
       // [ radius         ] // i
       // [ 1     2    3   ] // i+1
       // [ 0.25, 0.5, 1.5 ] // (i+1)/4
       float r = 0.25*(float(i) + 1.0);
       float a = sdSphere( p, r );
       d = opU( d, a );
       p.x -= 3.0*r;
    }
#endif


#if LESSON == 4 // Primitive: (Signed) Box
    // X = Width
    // Y = Height
    // Z = Depth
    d = sdBox( p, vec3(BOX_W,BOX_H,BOX_D), 0.0 ); // 0.0 = edge radius
#endif

#if LESSON == 5 // Primitive: (Unsigned) Round Box
    d = udRoundBox(p, vec3(BOX_W,BOX_H,BOX_D),0.1);
#endif


#if LESSON == 6 // Primitive: Torus
    d = sdTorus(p, vec2(BOX_D,BOX_H) ); // outer radius, inner radius
#endif


#if LESSON == 7 // Primitive: Cone
    d = sdCone( p, normalize(vec3( 1.0, 1.0, 2.0) ) );
#endif


#if LESSON == 8 // Primitive: Plane
    d = sdPlane( p, vec4( 0, 1, 0, 0 ) );
    gMaterial = MATERIAL_2;
#endif

#if LESSON == 9 // Primitive: Pyramid
    d = sdPyramid4( p, vec3( BOX_W, BOX_H, BOX_W ) );
#endif


#if LESSON == 10 // Primitive: Cylinder
    d = sdCappedCylinder( p, vec2( BOX_H, BOX_D ) ); // radius, height
#endif


#if LESSON == 11 // Primitive: Capsule
    d = sdCapsule( p,
        vec3(0.0 ,0.68,-0.7),
        vec3(0.02,0.07, 0.8),
        0.27
    );
#endif


#if LESSON == 12 // Composite: Capsule (sin + Box)
    p = q;
    //p.y += sin(p.y*PI/2.0)*0.25; // original *1.7)*0.3
    d = sdBox(
        vec3(0.0 ,0.68,-0.7) + p,
        vec3(0.02,0.07, 0.8),
        0.27
    );
#endif


#if LESSON == 13 // Distance:  Operation: Union (Box + Sphere)
    float d1 = sdBox( p, vec3(BOX_W,BOX_H,BOX_D), 0.0 );
    float d2 = sdSphere( p, RADIUS );
    d = opU( d1, d2 );
#endif


#if LESSON == 14 // Distance:  Operation: Subtraction (Box - Sphere)
    float d1 = sdBox( p, vec3(BOX_W,BOX_H,BOX_D), 0.0 );
    float d2 = sdSphere( p, RADIUS );
    d = opS( d1, d2 );
#endif


#if LESSON == 15 // Distance:  Operation: Intersection
    float d2 = sdBox( p, vec3(BOX_W,BOX_H,BOX_D), 0.0 );
    float d1 = sdSphere( p, RADIUS );
    d = opI( d1, d2 );
#endif


#if LESSON == 16 // Domain:    Operation: Translation (Box)
    vec3 t = vec3( BOX_W, BOX_D, -BOX_W*2. );
    q = opTrans( p, t ); // Loc4( t ) );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.01 );
#endif

#if LESSON == 17 // Domain:    Operation: Rotation (Box)
    // http://upload.wikimedia.org/wikipedia/commons/thumb/e/ed/30_degree_rotations_expressed_in_radian_measure.svg/400px-30_degree_rotations_expressed_in_radian_measure.svg.png
    // http://etc.usf.edu/clipart/43200/43216/unit-circle8_43216_lg.gif
    float angle = PI/4.0; // 45 degrees
    q = opTx( p, Rot4X( angle ) );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.01 );
#endif


#if LESSON == 18 // Domain:    Operation: Scale (Box)
    d = opScale( p, 1.0/4.0 );
#endif


#if LESSON == 19 // Domain:    Operation: Translation & Rotation (Box)
    vec3  t     = vec3( BOX_W, BOX_D, -BOX_W*2. );
    float angle = PI/8.0;
    q = opTx( p, Loc4( t ) );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.01 );
#endif


#if LESSON == 20 // Domain:    Operation: Translation & Rotation (Box)
    vec3  t     = vec3( BOX_W, BOX_D, -BOX_W*2. );
    float angle = PI/8.0;
    q = opTx( p, Rot4Z( angle ) * Loc4( t ) );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.01 );
#endif


#if LESSON == 21 // Domain:    Operation: Repetition (Box -> Cube)
    q = opRep( p, vec3(1.0) );
    d = sdSphere( q, BOX_H );
#endif


#if LESSON == 22 // Domain:    Operation: Repetition (Box -> Cube)
    q = opRep( p, vec3(1.0) );
    d = sdBox( q, vec3(BOX_H), 0.01 );
#endif


#if LESSON == 23 // Domain:    Deformation: (Cheap) Bend X
    q = opBendX( p, PI/4.0 );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.0 );
#endif


#if LESSON == 24 // Domain:    Deformation: (Cheap) Bend Y
    q = opBendY( p, PI/4.0 );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.0 );
#endif


#if LESSON == 25 // Domain:    Deformation: (Cheap) Bend Z
    q = opBendZ( p, PI/4.0 );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.0 );
#endif


#if LESSON == 26 // Domain:    Deformation: Twist X (Box)
    q = opTwistX( p, 4.0 );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.0 );
    gMaterial = MATERIAL_2;
#endif


#if LESSON == 27 // Domain:    Deformation: Twist Y (Torus)
    q = opTwistY( p, 5.0 );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.0 );
  //d = sdTorus(q,vec2(BOX_D,BOX_H)); // TODO: FIXME:
    gMaterial = MATERIAL_2;
#endif


#if LESSON == 28 // Domain:    Deformation: Twist Z (Box)
    q = opTwistZ( p, 4.0 );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.0 );
    gMaterial = MATERIAL_2;
#endif


#if LESSON == 29 // Distance:  Deformation: Displacement (Torus)
    float d1 = sdTorus(p, vec2(BOX_D,BOX_H) );
    float d2 = displacement(p, 1.25 ); // k = 1.25
    d = opDisplace( p, d1, d2 );
#endif


#if LESSON == 30 // Distance:  Deformation: Blend (Box + Torus)
    float d1 = sdBox( p, vec3(BOX_D,BOX_H,BOX_W), 0.0 ); // 0.0 = edge radius
    float d2 = sdTorus( p, vec2(BOX_D,BOX_H) );
    d = opBlend( d1, d2, 0.4 ); // d1^k d2^k, k = 0.4
#endif

/*
#if LESSON == 31 // Domain:    Deformation: Cheap Bend Z
    q = opCheapBend( p, PI/4.0 );
    d = sdBox( q, vec3(BOX_W,BOX_H,BOX_D), 0.0 );
#endif
*/

#if LESSON == 0
    // - - - 8< - - -
    d = return 0; // Masterpiece creative time!
    // - - - 8< - - -
#endif


#if LESSON == 101 // Empty Glass
    // https://www.shadertoy.com/view/4s2GDV
    // df_obj()
    // Slightly cleaned up
    // p *= 1.25; // Sadly we can't scale down as we run into precision issues...

    float a  = (length(p.xz)-1.0-p.y*.15)*.85,
          a2 = (length(p.xz)-0.9-p.y*.15)*.85;
          a  = max(abs(p.y)-1.0,a);
          a  = max(a,-max(-.8-p.y,a2));
          a  = max(a,-length(p+vec3(.0,4.0,.0))+3.09);
    //a = a; // WTF?

    vec3 p2 = p;
    p2.xz*=(1.0-p.y*.15);
    float angle = atan(p2.x,p2.z);
    float mag = length(p2.xz);
    angle = mod(angle,PI*.125)-PI*.125*.5;
    p2.xz = vec2(cos(angle),sin(angle))*mag;
    a     = max(a,(-length(p2+vec3(-7.0,0.0,0.0))+6.05)*.85);

    d = a;
#endif


#if LESSON == 102 // Crystal Skull
    // https://www.shadertoy.com/view/MsS3WV
    // skull()
    // replaced 'float d = ...' with 'd = ...'
    p.y += sin(p.y*1.6)*0.2;
    p.z -= p.x*0.05;
    float e = sdTorus(p+vec3(-0.4,0.0,0.0),vec2(0.3,0.1));   //eye

    p.z = q.z;
    p.z+=p.x*0.05;
    float f = sdTorus(p+vec3( 0.4,0.0,0.0),vec2(0.3,0.1));   //eye
    p.x+=sin(p.x);
    float n = sdTorus(p+vec3( 0.0,0.45,0.19),vec2(0.2,0.05));  //nose

    p = q;
    p.x+=sin(p.x*0.07);
    p.x*= cos(p.y*0.6+abs(cos(3.7+p.y)*0.2)*1.1) ;
    float s = length(p+vec3( 0.0,-0.14,-0.79))-0.98; //back

    p = q;
    p.y += sin(p.y*1.7)*0.3;
    d = length(p+vec3(-0.4,0.0,0.1))-0.25; //eyehole
        s = max(s,-d);
            d = length(p+vec3( 0.4,0.0,0.1))-0.25;  //eyehole
        s = max(s,-d);

    p = q;
    p.z += p.z-p.y*0.4;
    float v = sdBox(p+vec3(0.0,0.68,-0.7),vec3(0.02,0.07,0.8), 0.27);   //chin
    float o = sMinP(e,f,0.5);
    o = sMinE(o,n,14.0);
    o = sMinP(o,s,0.09);
    d = sMinE(o,v,12.0);
#endif

    return d;
}

// ========================================
vec4 DE( vec3 hp, float fts ) {
    vec4 vResult = vec4(MAX_DIST, -1.0, 0.0, 0.0);
    vec4 vDist = vec4( draw(hp), MATERIAL_1, hp.xz);
    vDist.y = gMaterial; // v0.42 draw may over-ride material
    return opUt(vResult, vDist, fts);
}


// ========================================
sMat getMaterial( sHit hitInfo ) {
    sMat mat;
    if(hitInfo.oid.x == MATERIAL_1) {
        mat.frs = 0.31;
        mat.smt = 1.0;
        mat.trs = 1.0;
        mat.fri = 0.75;
        const float fExtinctionScale = 2.0;
        vec3 tc = vec3(0.93,0.96,1.0);        //tex/col
        mat.ctc = (vec3(1.0) - tc) * fExtinctionScale; 
    } else
    if(hitInfo.oid.x == MATERIAL_2) {
        mat.frs = 0.0;
        mat.smt = 1.0;
        mat.trs = 0.0;
        mat.fri = 0.0;
        mat.ctc = vec3(0.25,0.5,0.75); // Beautiful Baby Blue
    }
    return mat;
}

// ========================================
vec3 getBackground( vec3 rd ) {
#ifdef BACKGROUND_BLUE
    const vec3  tc = vec3(0.65, 0.78, 1.0);
    const vec3  cc = tc * 0.5;
          float f  = clamp(rd.y, 0.0, 1.0);
    return mix(cc, tc, f);
#else
    return texture(iChannel0, rd).xyz;
#endif
}

// ========================================
sLight getDirLight() {
    sLight result;
    result.rd  = normalize(vec3(-0.2, -0.3, 0.5));
    result.col = vec3(8.0, 7.5, 7.0);
    return result;
}

// ========================================
vec3 getAmbient( vec3 nor ) {
    return getBackground(nor);
}

// ========================================
vec3 normal( vec3 p, float fts ) {
    vec3 e = vec3(0.01,-0.01,0.0);
    return normalize( vec3(
        e.xyy*DE(p+e.xyy,fts).x +
        e.yyx*DE(p+e.yyx,fts).x +
        e.yxy*DE(p+e.yxy,fts).x +
        e.xxx*DE(p+e.xxx,fts).x)
    );
}

// ========================================
void march( sRay ray, out sHit res, int maxIter, float fts ) {
    res.hd = ray.sd;
    res.oid.x = 0.0;

    for( int i=0;i<=MARCHSTEPS;i++ ) {
        res.hp = ray.ro + ray.rd * res.hd;
        vec4 r = DE( res.hp, fts );
        res.oid = r.yzw;
        if((abs(r.x) <= 0.01) || (res.hd >= ray.rl) || (i > maxIter))
            break;
        res.hd = res.hd + r.x;
    }
    if(res.hd >= ray.rl) {
        res.hd = MAX_DIST;
        res.hp = ray.ro + ray.rd * res.hd;
        res.oid.x = 0.0;
    }
}

// ========================================
float getShadow( vec3 hp, vec3 nor, vec3 lrd, float d ) {
#ifdef SHADOWS
    sRay ray;
    ray.rd = lrd;
    ray.ro = hp;
    ray.sd = 0.05 / abs(dot(lrd, nor));
    ray.rl = d - ray.sd;
    sHit si;
    march(ray, si, 32, kNt);
    float s = step(0.0, si.hd) * step(d, si.hd );
    return s;
#else
    return 1.0;
#endif
}

// ========================================
float getAmbientOcclusion( sHit hi, sSurf s ) {
#ifdef AMBIENT_OCCLUSION
    vec3 hp = hi.hp;
    vec3 nor = s.nor;
    float ao = 1.0;

    float d = 0.0;
    for( int i=0; i<=5; i++ ) {
        d += 0.1;
        vec4 r = DE(hp + nor * d, kNt);
        ao *= 1.0 - max(0.0, (d - r.x) * 0.2 / d );
    }
    return ao;
#else
    return 1.0;
#endif
}

// ========================================
vec3 getFog( vec3 color, sRay ray, sHit hi ) {
#ifdef FOG
    float a = exp(hi.hd * - 0.05);
    vec3 fog = getBackground(ray.rd);

    #ifdef DIRECTIONAL_LIGHT_FLARE
        sLight lig = getDirLight();
        float f = clamp(dot(-lig.rd, ray.rd), 0.0, 1.0);
        fog += lig.col * pow(f, 10.0);
    #endif 

    color = mix(fog, color, a);
#endif

    return color;
}

// http://en.wikipedia.org/wiki/Schlick's_approximation
// Anisotropic scattering Schlick phase function
// "Interactive Manycore Photon Mapping"
// See: https://www.scss.tcd.ie/publications/tech-reports/reports.11/TCD-CS-2011-04.pdf
//
// More complex empirically motivated phase functions are efficiently approximated by the Schluck function [BLS93].
// ========================================
float getSchlick(vec3 nor, vec3 v, float frs, float sf) {
    float f = dot(nor, -v);
    f = clamp((1.0 - f), 0.0, 1.0);
    float fDotPow = pow(f, 5.0);
    return frs + (1.0 - frs) * fDotPow * sf;
}

// http://en.wikipedia.org/wiki/Fresnel_equations
// ========================================
vec3 getFresnel( vec3 dif, vec3 spe, vec3 nor, vec3 v, sMat m ) {
    float f = getSchlick(nor, v, m.frs, m.smt * 0.9 + 0.1);
    return mix(dif, spe, f);
}

// ========================================
float getPhong( vec3 ird, vec3 lrd, vec3 nor, float smt ) {
    vec3  v  = normalize(lrd - ird);
    float f  = max(0.0, dot(v, nor));
    float sp = exp2(4.0 + 6.0 * smt);
    float si = (sp + 2.0) * 0.125;
    return pow(f, sp) * si;
}

// ========================================
sShade setDirLight( sLight l, vec3 p, vec3 d, vec3 nor, sMat m ) {
    sShade s;
    vec3 lrd = -l.rd;
    float sf = getShadow( p, nor, lrd, 8.0 );
    vec3 il = l.col * sf * max(0.0, dot(lrd, nor));
    s.dfs = il;
    s.spc = getPhong( d, lrd, nor, m.smt ) * il;
    return s;
}

// ========================================
vec3 setColor( sRay ray, sHit hi, sSurf sc, sMat m ) {
    vec3 color;
    sShade s;
    s.dfs = vec3(0.0);
    s.spc = vec3(0.0);
    float ao = getAmbientOcclusion(hi, sc);
    vec3 al = getAmbient(sc.nor) * ao;
    s.dfs += al;
    s.spc += sc.ref;

#ifdef DIRECTIONAL_LIGHT
    sLight dl = getDirLight();
    sShade sh = setDirLight(dl, hi.hp, ray.rd, sc.nor, m);
    s.dfs += sh.dfs;
    s.spc += sh.spc;
#endif

    vec3 dr = s.dfs * m.ctc;

    dr = mix(dr, sc.tra, m.trs);

#ifdef SPECULAR
    color = getFresnel(dr , s.spc, sc.nor, ray.rd, m);
#else
    color = dr;
#endif

    return color;
}

// ========================================
vec3 getColor( sRay ray ) {
    sHit hi;
    march(ray, hi, 32, kNt);
    vec3 color;

    if(hi.oid.x < 0.5) {
        color = getBackground(ray.rd);
    } else {
        sSurf s;
        s.nor  = normal(hi.hp, kNt);
        sMat m = getMaterial( hi );
        s.ref  = getBackground(reflect(ray.rd, s.nor));
        m.trs  = 0.0;
        color  = setColor(ray, hi, s, m);
    }

    color = getFog(color, ray, hi);
    return color;
}

// ========================================
vec3 getReflection( sRay ray, sHit hitInfo, sSurf s ) {
#ifdef REFLECTIONS
    sRay rRay;
    rRay.rd = reflect(ray.rd, s.nor);
    rRay.ro = hitInfo.hp;
    rRay.rl = 16.0;
    rRay.sd = 0.1 / abs(dot(rRay.rd, s.nor));
    return getColor(rRay);
#else
    return getBackground(reflect(ray.rd, s.nor));
#endif
}

// ========================================
vec3 getTransparency( sRay ray, sHit hit, sSurf s, sMat m ) {
#ifdef TRANSPARENCY
    sRay rRay;
    rRay.rd = refract(ray.rd, s.nor, m.fri);
    rRay.ro = hit.hp;
    rRay.rl = 16.0;
    rRay.sd = 0.05 / abs(dot(rRay.rd, s.nor));

    #ifdef DOUBLE_SIDED_TRANSPARENCY
        sHit hit2;
        march(rRay, hit2, 32, kIt);
        vec3 nor = normal(hit2.hp, kIt);
            sRay rRay2;
            rRay2.rd = refract(rRay.rd, nor, 1.0 / m.fri);
            rRay2.ro = hit2.hp;
            rRay2.rl = 16.0;
            rRay2.sd = 0.0;
        float ed = hit2.hd;
        vec3 color = getColor( rRay2 );
    #else
        vec3 color = getColor( rRay );
        float ed = 0.5;
    #endif

    return color * clamp(exp(-(m.ctc * ed)),0.0,1.0);
#else
    return getBackground(reflect(ray.rd, s.nor));
#endif
}

// ========================================
vec3 getRayColor( sRay ray ) {
    sHit i;
    march(ray, i, MARCHSTEPS, kTt); //256

    vec3 color;
    if(i.oid.x < 0.5) {
        color = getBackground(ray.rd);
    } else  {
        sSurf s;
        s.nor  = normal(i.hp, kTt);
        sMat m = getMaterial( i );
        s.ref  = getReflection(ray, i, s);
        if(m.trs > 0.0) s.tra = getTransparency(ray, i, s, m);
        color  = setColor(ray, i, s, m);
    }

    getFog(color, ray, i); // BUG? Is this intentional that color is not updated??
    return color;
}

// ========================================
sRay setCameraRay( vec3 hp, vec3 i , vec2 fragCoord) {
    float fRatio = iResolution.x / iResolution.y; // Aspect Ratio

    vec3 f   = normalize(i - hp);
    vec3 vUp = vec3(0.0, 1.0, 0.0);
    vec2 vvc = 2.*fragCoord.xy/iResolution.xy-1.;
    vvc.y /= fRatio;

    sRay ray;
    ray.ro = hp;
    vec3 r = normalize(cross(f, vUp));
    vUp    = cross(r, f);
    ray.rd = normalize( r * vvc.x + vUp * vvc.y + f);
    ray.sd = 0.0;
    ray.rl = MAX_DIST;
    return ray;
}

// ========================================
void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec2 m = vec2(0.0); // Default OpenGL camera: Look down -z axis

#ifdef VIEW_ISOMETRIC
    m = vec2( 3.5, 1.0 ) / PI; // fake isoemetric
#endif

#ifdef VIEW_ZERO
    // m.x = 0.0; // +z // h -> 0      =   0
    // m.x =+1.0;; //-z // h -> PI     = 180
    // m.x = 0.5; // +x // h -> PI  /2 =  90
    // m.x =-0.5; // -x // h -> PI*3/2 = 270
    //m.y = iMouse.y / iResolution.y; // uncomment to allow Y rotation
#else
    m += 2.* iMouse.xy / iResolution.xy;
    m.x += 1.;
#endif

    float nRotate = 0.0; // no rotation
#ifdef AUTO_ROTATE
    nRotate = iGlobalTime *0.05; // slow rotation
#endif

    //float h  = mix(0.0, PI , m.x - nRotate);
    float h  = PI * (m.x - nRotate);
    float e  = mix(0.0, 2.5, m.y                ); // eye
    // Hold down mouse button to zoom out & rotate the camera!
    float d  = mix(2.5, 2.5 + (iMouse.z > 0.0 ? 4.0 : 2.0), m.y); // eye distance

    // ro RayOrigin
    vec3 ro  = vec3(sin(h) * cos(e), sin(e), cos(h) * cos(e)) * d;
    vec3 ta  = vec3(0.0, 0.0, 0.0);

    sRay ray = setCameraRay( ta + ro, ta, fragCoord);
    vec3 col = getRayColor( ray );
    fragColor = vec4( col, 1.0 );
}

