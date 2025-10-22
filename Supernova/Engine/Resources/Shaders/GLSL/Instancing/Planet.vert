#version 460

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
	vec4 viewPos;
	vec4 lightPos;
	float lightIntensity;
} ubo;

layout (push_constant) uniform Push
{
	mat4 model;
} push;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;
layout (location = 5) out float outLightIntensity;

void main() 
{
	outColor = inColor;
	outUV = inUV;

	mat4 modelView = (ubo.view * push.model);
	gl_Position = ubo.projection * modelView * vec4(inPos.xyz, 1.0);
	
	vec4 pos = modelView * vec4(inPos, 1.0);
	outNormal = mat3(modelView) * inNormal;
	vec3 lPos = mat3(modelView) * ubo.lightPos.xyz;
	outLightVec = lPos - pos.xyz;
	outViewVec = ubo.viewPos.xyz - pos.xyz;
	outLightIntensity = ubo.lightIntensity;
}
