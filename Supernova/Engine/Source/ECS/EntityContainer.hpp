#pragma once

#include "UniqueIdentifier.hpp"

#include <entt.hpp>
#include <unordered_map>

namespace ECS
{
	struct EntityContainer
	{
		entt::registry mRegistry;
		std::unordered_map<UniqueIdentifier, entt::entity> mEntityMap;
	};
}
