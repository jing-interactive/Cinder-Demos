#version 150

in vec3    vNormal;
out vec4 	oColor;

void main( void )
{
	float v = dot(vNormal, vec3(0,1,0)) + 0.2;
	oColor = vec4(v, v, v,1);
}