#version 460

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
	vec4 viewPos;
	vec4 lightPos;
} ubo;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;

void main() 
{
	outUV = inUV;

	gl_Position = ubo.projection * ubo.modelview * vec4(inPos.xyz, 1.0);

    vec4 pos = ubo.modelview * vec4(inPos, 1.0);
	outNormal = mat3(inverse(transpose(ubo.modelview))) * inNormal;
	vec3 lPos = mat3(ubo.modelview) * ubo.lightPos.xyz;
    outLightVec = lPos - pos.xyz;
    outViewVec = ubo.viewPos.xyz - pos.xyz;
}
