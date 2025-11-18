#pragma once

#include "UniqueIdentifier.hpp"

#include <string>

namespace ECS
{
	struct EntityContainer;
	class Entity;

	class Scene
	{
		friend class Entity;

	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string& aName = std::string());
		Entity CreateEntity(UniqueIdentifier aUniqueIdentifier, const std::string& aName = std::string());
		void DestroyEntity(Entity aEntity);
		Entity DuplicateEntity(Entity aEntity);
		Entity FindEntityByName(std::string_view aName);

		template<typename... Components>
		auto GetAllEntitiesWith();

		EntityContainer* GetEntityContainer();
		EntityContainer* GetEntityContainer() const;

	private:
		template<typename T>
		void OnComponentAdded(Entity aEntity, T& aComponent);

	private:
		EntityContainer* mEntityContainer;
	};
}
