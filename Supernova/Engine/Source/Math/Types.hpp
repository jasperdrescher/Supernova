#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Math
{
	using Vector2f = glm::vec2;
	using Vector3f = glm::vec3;
	using Vector4f = glm::vec4;
	using Matrix4f = glm::mat4;
	using Quaternionf = glm::quat;
}
