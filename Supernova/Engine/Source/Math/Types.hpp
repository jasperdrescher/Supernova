#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtx/quaternion.hpp>
#include <glm/mat2x2.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Math
{
	using Vector2f = glm::vec2; // 2D floating-point vector
	using Vector3f = glm::vec3; // 3D floating-point vector
	using Vector4f = glm::vec4; // 4D floating-point vector

	using Matrix2f = glm::mat2; // 2D floating-point matrix
	using Matrix3f = glm::mat3; // 3D floating-point matrix
	using Matrix4f = glm::mat4; // 4D floating-point matrix

	using Quaternionf = glm::quat; // Floating-point quaternion
}
