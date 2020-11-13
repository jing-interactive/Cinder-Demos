in vec3    vNormal;
in vec2    vTexCoord0;
out vec4 	oColor;
uniform sampler2D u_BaseColorSampler;

void main( void )
{
	float v = dot(vNormal, vec3(0,1,0)) + 0.2;
    vec4 baseColor = texture(u_BaseColorSampler, vTexCoord0);
	// baseColor *= v;
	oColor = baseColor;
}