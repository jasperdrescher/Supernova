#include "Entity.hpp"

#include "Components.hpp"
#include "EntityContainer.hpp"
#include "Scene.hpp"
#include "UniqueIdentifier.hpp"

#include <cassert>
#include <cstdint>
#include <entt.hpp>
#include <string>

namespace ECS
{
	Entity::Entity()
		: mEntityHandle{entt::null}
		, mScene{nullptr}
	{
		assert(mScene); // Always assert as this ctor should not be used
	}

	Entity::Entity(entt::entity aHandle, Scene* aScene)
		: mEntityHandle(aHandle), mScene(aScene)
	{
	}

	template<typename T, typename... Args>
	T& Entity::AddComponent(Args&&... aArgs)
	{
		assert(!HasComponent<T>());
		T& component = mScene->GetEntityContainer()->mRegistry.emplace<T>(mEntityHandle, std::forward<Args>(aArgs)...);
		mScene->OnComponentAdded<T>(*this, component);
		return component;
	}

	template<typename T, typename... Args>
	T& Entity::AddOrReplaceComponent(Args&&... aArgs)
	{
		T& component = mScene->GetEntityContainer()->mRegistry.emplace_or_replace<T>(mEntityHandle, std::forward<Args>(aArgs)...);
		mScene->OnComponentAdded<T>(*this, component);
		return component;
	}

	template<typename T>
	T& Entity::GetComponent()
	{
		assert(HasComponent<T>());
		return mScene->GetEntityContainer()->mRegistry.get<T>(mEntityHandle);
	}

	template<typename T>
	const T& Entity::GetComponent() const
	{
		assert(HasComponent<T>());
		return mScene->GetEntityContainer()->mRegistry.get<T>(mEntityHandle);
	}

	template<typename T>
	const bool Entity::HasComponent() const
	{
		return mScene->GetEntityContainer()->mRegistry.all_of<T>(mEntityHandle);
	}

	template<typename T>
	void Entity::RemoveComponent()
	{
		assert(HasComponent<T>());
		mScene->GetEntityContainer()->mRegistry.remove<T>(mEntityHandle);
	}

	UniqueIdentifier Entity::GetUniqueIdentifier() const
	{
		return GetComponent<IdentifierComponent>().mUniqueIdentifier;
	}

	Entity::operator bool() const
	{
		return mEntityHandle != entt::null;
	}

	Entity::operator std::uint32_t() const
	{
		return static_cast<std::uint32_t>(mEntityHandle);
	}

	const std::string& Entity::GetName() const
	{
		return GetComponent<TagComponent>().mTag;
	}

	bool Entity::operator==(const Entity& aOther) const
	{
		return mEntityHandle == aOther.mEntityHandle && mScene == aOther.mScene;
	}
}
