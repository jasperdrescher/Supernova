#version 460

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;

layout (location = 3) in vec3 instancePos;
layout (location = 4) in float instanceScale;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view;
	vec4 viewPos;
	vec4 lightPos;
	vec4 frustumPlanes[6];
	float lightIntensity;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out float outLightIntensity;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	outColor = inColor;
	outNormal = inNormal;
	
	vec4 pos = vec4((inPos.xyz * instanceScale) + instancePos, 1.0);

	gl_Position = ubo.projection * ubo.view * pos;
	
	vec3 lPos = mat3(ubo.view) * ubo.lightPos.xyz;
	outLightVec = lPos - pos.xyz;
	outViewVec = ubo.viewPos.xyz - pos.xyz;
	outLightIntensity = ubo.lightIntensity;
}
