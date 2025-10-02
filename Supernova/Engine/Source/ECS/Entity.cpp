#include "Entity.hpp"

#include "Scene.hpp"

#include <entt.hpp>

namespace ECS
{
	ECS::Entity::Entity(entt::entity aHandle, Scene* aScene)
		: mEntityHandle(aHandle), mScene(aScene)
	{
	}
}
