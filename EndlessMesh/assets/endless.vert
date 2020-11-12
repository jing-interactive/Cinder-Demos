#version 150 core

in vec4		ciPosition;
in vec3		ciNormal;
uniform vec3 uPlayerPos;
uniform float uRollStrength;
uniform mat4 ciModelMatrix;
uniform mat4 ciViewProjection;
out vec3    vNormal;

void main()
{
    vec4 worldPos = ciModelMatrix  * ciPosition;
#if 0
    gl_Position = ciViewProjection * worldPos;
#else
    // vec2 camXZ = vec2(ciModelMatrix[3][0], ciModelMatrix[3][2]);
    vec2 diff = worldPos.xz - uPlayerPos.xz;
    float dist2Player = /*diff.x * diff.x + */diff.y * diff.y;
    worldPos.y -= dist2Player * uRollStrength * 0.00001;
    gl_Position = ciViewProjection * worldPos; // needs w for proper perspective correction
#endif
    vNormal = ciNormal;
}
