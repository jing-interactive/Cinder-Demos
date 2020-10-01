#version 150

uniform sampler2D image;
uniform sampler2D diffuse;

in vec3 cI; // incident ray, camera space
in vec3 cN; // normal, camera space
in vec2 TexCoord0;
 
out vec4 fragColor;

void main()
{
	vec3 r = reflect( cI, normalize(cN) );  r.z += 1.0;
	float m = 0.5 * inversesqrt( dot( r, r ) );
	vec2 uv = r.xy * m + 0.5;
	vec4 clr = texture( diffuse, TexCoord0 );
	fragColor.rgb = clr.rgb * texture( image, uv ).rgb;
	fragColor.a = clr.a;
}