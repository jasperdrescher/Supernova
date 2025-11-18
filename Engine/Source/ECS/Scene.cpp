#include "Scene.hpp"

#include "Components.hpp"
#include "Entity.hpp"
#include "EntityContainer.hpp"
#include "UniqueIdentifier.hpp"

#include <string>

namespace SceneLocal
{
	template<typename... Component>
	static void CopyComponentIfExists(ECS::Entity aDst, ECS::Entity aSrc)
	{
		([&]()
			{
				if (aSrc.HasComponent<Component>())
					aDst.AddOrReplaceComponent<Component>(aSrc.GetComponent<Component>());
			}(), ...);
	}

	template<typename... Component>
	static void CopyComponentIfExists(ECS::ComponentGroup<Component...>, ECS::Entity aDst, ECS::Entity aSrc)
	{
		CopyComponentIfExists<Component...>(aDst, aSrc);
	}
}

namespace ECS
{
	Scene::Scene()
		: mEntityContainer{nullptr}
	{
		mEntityContainer = new EntityContainer();
	}

	Scene::~Scene()
	{
		delete mEntityContainer;
	}

	Entity Scene::CreateEntity(const std::string& aName)
	{
		return CreateEntity(UniqueIdentifier(), aName);
	}

	Entity Scene::CreateEntity(UniqueIdentifier aUniqueIdentifier, const std::string& aName)
	{
		Entity entity = {mEntityContainer->mRegistry.create(), this};
		entity.AddComponent<IdentifierComponent>(aUniqueIdentifier);
		entity.AddComponent<TransformComponent>();

		TagComponent& tagComponent = entity.AddComponent<TagComponent>();
		tagComponent.mTag = aName.empty() ? "Entity" : aName;

		mEntityContainer->mEntityMap[aUniqueIdentifier] = entity;

		return entity;
	}

	void Scene::DestroyEntity(Entity aEntity)
	{
		mEntityContainer->mEntityMap.erase(aEntity.GetUniqueIdentifier());
		mEntityContainer->mRegistry.destroy(aEntity);
	}

	Entity Scene::DuplicateEntity(Entity aEntity)
	{
		const std::string name = aEntity.GetName();
		Entity newEntity = CreateEntity(name);
		SceneLocal::CopyComponentIfExists(AllComponents{}, newEntity, aEntity);
		return newEntity;
	}

	Entity Scene::FindEntityByName(std::string_view aName)
	{
		const auto view = mEntityContainer->mRegistry.view<TagComponent>();
		for (const auto entity : view)
		{
			const TagComponent& tc = view.get<TagComponent>(entity);
			if (tc.mTag == aName)
				return Entity{entity, this};
		}
		return {};
	}

	EntityContainer* Scene::GetEntityContainer()
	{
		return mEntityContainer;
	}

	EntityContainer* Scene::GetEntityContainer() const
	{
		return mEntityContainer;
	}

	template<typename ...Components>
	auto Scene::GetAllEntitiesWith()
	{
		return mEntityContainer->mRegistry.view<Components...>();
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
