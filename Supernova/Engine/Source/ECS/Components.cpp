#include "Components.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat4x4.hpp>

namespace ECS
{
	glm::mat4 TransformComponent::GetTransform() const
	{
		const glm::mat4 rotation = glm::toMat4(glm::quat(mRotation));

		return glm::translate(glm::mat4{1.0f}, mPosition)
			* rotation
			* glm::scale(glm::mat4{1.0f}, mScale);
	}
}
