#pragma once

#include "UniqueIdentifier.hpp"

#include <entt.hpp>
#include <string>
#include <unordered_map>

namespace ECS
{
	class Entity;

	class Scene
	{
		friend class Entity;

	public:
		Entity CreateEntity(const std::string& aName = std::string());
		Entity CreateEntity(UniqueIdentifier aUniqueIdentifier, const std::string& aName = std::string());
		void DestroyEntity(Entity aEntity);
		Entity DuplicateEntity(Entity aEntity);
		Entity FindEntityByName(std::string_view aName);

		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return mRegistry.view<Components...>();
		}
	private:
		template<typename T>
		void OnComponentAdded(Entity aEntity, T& aComponent);

	private:
		entt::registry mRegistry;
		std::unordered_map<UniqueIdentifier, entt::entity> mEntityMap;
	};
}
