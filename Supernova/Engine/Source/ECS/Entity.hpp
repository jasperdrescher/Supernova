#pragma once

#include "Components.hpp"
#include "Scene.hpp"
#include "UniqueIdentifier.hpp"

#include <cassert>
#include <cstdint>
#include <entt.hpp>
#include <string>
#include <utility>

namespace ECS
{
	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity aHandle, Scene* aScene);
		Entity(const Entity& aOthehr) = default;

		template<typename T, typename... Args>
		T& AddComponent(Args&&... aArgs)
		{
			assert(!HasComponent<T>());
			T& component = mScene->mRegistry.emplace<T>(mEntityHandle, std::forward<Args>(aArgs)...);
			mScene->OnComponentAdded<T>(*this, component);
			return component;
		}

		template<typename T, typename... Args>
		T& AddOrReplaceComponent(Args&&... args)
		{
			T& component = mScene->mRegistry.emplace_or_replace<T>(mEntityHandle, std::forward<Args>(args)...);
			mScene->OnComponentAdded<T>(*this, component);
			return component;
		}

		template<typename T>
		T& GetComponent()
		{
			assert(HasComponent<T>());
			return mScene->mRegistry.get<T>(mEntityHandle);
		}

		template<typename T>
		const T& GetComponent() const
		{
			assert(HasComponent<T>());
			return mScene->mRegistry.get<T>(mEntityHandle);
		}

		template<typename T>
		const bool HasComponent() const
		{
			return mScene->mRegistry.all_of<T>(mEntityHandle);
		}

		template<typename T>
		void RemoveComponent()
		{
			assert(HasComponent<T>());
			mScene->mRegistry.remove<T>(mEntityHandle);
		}

		UniqueIdentifier GetUniqueIdentifier() const { return GetComponent<IDComponent>().mUniqueIdentifier; }
		operator bool() const { return mEntityHandle != entt::null; }
		operator entt::entity() const { return mEntityHandle; }
		operator std::uint32_t() const { return (std::uint32_t)mEntityHandle; }

		const std::string& GetName() const { return GetComponent<TagComponent>().mTag; }

		bool operator==(const Entity& aOther) const
		{
			return mEntityHandle == aOther.mEntityHandle && mScene == aOther.mScene;
		}

		bool operator!=(const Entity& aOther) const
		{
			return !(*this == aOther);
		}

	private:
		entt::entity mEntityHandle{entt::null};
		Scene* mScene = nullptr;
	};
}
