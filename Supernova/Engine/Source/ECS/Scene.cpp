#include "Scene.hpp"

#include "Components.hpp"
#include "Entity.hpp"
#include "UniqueIdentifier.hpp"

#include <string>

namespace ECS
{
	template<typename... Component>
	static void CopyComponentIfExists(Entity aDst, Entity aSrc)
	{
		([&]()
			{
				if (aSrc.HasComponent<Component>())
					aDst.AddOrReplaceComponent<Component>(aSrc.GetComponent<Component>());
			}(), ...);
	}

	template<typename... Component>
	static void CopyComponentIfExists(ComponentGroup<Component...>, Entity aDst, Entity aSrc)
	{
		CopyComponentIfExists<Component...>(aDst, aSrc);
	}

	Entity ECS::Scene::CreateEntity(const std::string& aName)
	{
		return CreateEntityWithUUID(UniqueIdentifier(), aName);
	}

	Entity ECS::Scene::CreateEntityWithUUID(UniqueIdentifier aUniqueIdentifier, const std::string& aName)
	{
		Entity entity = {mRegistry.create(), this};
		entity.AddComponent<IdentifierComponent>(aUniqueIdentifier);
		entity.AddComponent<TransformComponent>();

		TagComponent& tagComponent = entity.AddComponent<TagComponent>();
		tagComponent.mTag = aName.empty() ? "Entity" : aName;

		mEntityMap[aUniqueIdentifier] = entity;

		return entity;
	}

	void ECS::Scene::DestroyEntity(Entity aEntity)
	{
		mEntityMap.erase(aEntity.GetUniqueIdentifier());
		mRegistry.destroy(aEntity);
	}

	Entity ECS::Scene::DuplicateEntity(Entity aEntity)
	{
		const std::string name = aEntity.GetName();
		Entity newEntity = CreateEntity(name);
		CopyComponentIfExists(AllComponents{}, newEntity, aEntity);
		return newEntity;
	}

	Entity ECS::Scene::FindEntityByName(std::string_view aName)
	{
		auto view = mRegistry.view<TagComponent>();
		for (auto entity : view)
		{
			const TagComponent& tc = view.get<TagComponent>(entity);
			if (tc.mTag == aName)
				return Entity{entity, this};
		}
		return {};
	}

	template<typename T>
	void Scene::OnComponentAdded(Entity /*aEntity*/, T& /*aComponent*/)
	{
		static_assert(sizeof(T) == 0);
	}

	template<>
	void Scene::OnComponentAdded<IdentifierComponent>(Entity /*aEntity*/, IdentifierComponent& /*aComponent*/)
	{
	}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity /*aEntity*/, TransformComponent& /*aComponent*/)
	{
	}

	template<>
	void Scene::OnComponentAdded<TagComponent>(Entity /*aEntityaEntity*/, TagComponent& /*aComponent*/)
	{
	}
}
