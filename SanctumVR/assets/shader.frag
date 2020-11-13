#version 150
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_KHR_vulkan_glsl : enable

layout (set=1, binding=0) uniform sampler2D uTex0;

layout (std140, set=1, binding=1) uniform ciBlock1T {
	uniform vec4 uAmbient;
} ciBlock1;

layout (location = 0) in vec2 TexCoord;

layout (location = 0) out vec4 oColor;

void main( void )
{
	oColor = texture( uTex0, vec2(TexCoord.s, 1.0-TexCoord.t) ) * ciBlock1.uAmbient;
}
