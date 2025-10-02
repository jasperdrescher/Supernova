#include "Entity.hpp"

#include "Scene.hpp"

#include <cassert>
#include <entt.hpp>

namespace ECS
{
	Entity::Entity(entt::entity aHandle, Scene* aScene)
		: mEntityHandle(aHandle), mScene(aScene)
	{
	}

	template<typename T, typename... Args>
	T& Entity::AddComponent(Args&&... aArgs)
	{
		assert(!HasComponent<T>());
		T& component = mScene->mRegistry.emplace<T>(mEntityHandle, std::forward<Args>(aArgs)...);
		mScene->OnComponentAdded<T>(*this, component);
		return component;
	}

	template<typename T, typename... Args>
	T& Entity::AddOrReplaceComponent(Args&&... aArgs)
	{
		T& component = mScene->mRegistry.emplace_or_replace<T>(mEntityHandle, std::forward<Args>(aArgs)...);
		mScene->OnComponentAdded<T>(*this, component);
		return component;
	}

	template<typename T>
	T& Entity::GetComponent()
	{
		assert(HasComponent<T>());
		return mScene->mRegistry.get<T>(mEntityHandle);
	}

	template<typename T>
	const T& Entity::GetComponent() const
	{
		assert(HasComponent<T>());
		return mScene->mRegistry.get<T>(mEntityHandle);
	}

	template<typename T>
	const bool Entity::HasComponent() const
	{
		return mScene->mRegistry.all_of<T>(mEntityHandle);
	}

	template<typename T>
	void Entity::RemoveComponent()
	{
		assert(HasComponent<T>());
		mScene->mRegistry.remove<T>(mEntityHandle);
	}

	bool Entity::operator==(const Entity& aOther) const
	{
		return mEntityHandle == aOther.mEntityHandle && mScene == aOther.mScene;
	}
}
