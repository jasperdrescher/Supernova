#pragma once

#include "UniqueIdentifier.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <string>

namespace ECS
{
	struct IdentifierComponent
	{
		IdentifierComponent() = default;
		IdentifierComponent(const IdentifierComponent&) = default;
		IdentifierComponent(UniqueIdentifier aUniqueIdentifier)
			: mUniqueIdentifier(aUniqueIdentifier)
		{
		}

		UniqueIdentifier mUniqueIdentifier;
	};

	struct TagComponent
	{
		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: mTag(tag)
		{
		}

		std::string mTag;
	};

	struct TransformComponent
	{
		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& aPosition)
			: mPosition(aPosition)
		{
		}

		glm::mat4 GetTransform() const;

		glm::vec3 mPosition = {0.0f, 0.0f, 0.0f};
		glm::vec3 mRotation = {0.0f, 0.0f, 0.0f};
		glm::vec3 mScale = {1.0f, 1.0f, 1.0f};
	};

	template<typename... Component>
	struct ComponentGroup
	{
	};

	using AllComponents =
		ComponentGroup<TransformComponent>;
}
