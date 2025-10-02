#pragma once

#include "Components.hpp"
#include "UniqueIdentifier.hpp"

#include <cstdint>
#include <entt.hpp>
#include <string>
#include <utility>

namespace ECS
{
	class Scene;

	class Entity
	{
	public:
		Entity() = default;
		Entity(entt::entity aHandle, Scene* aScene);
		Entity(const Entity& aOther) = default;

		template<typename T, typename... Args>
		T& AddComponent(Args&&... aArgs);

		template<typename T, typename... Args>
		T& AddOrReplaceComponent(Args&&... aArgs);

		template<typename T>
		T& GetComponent();

		template<typename T>
		const T& GetComponent() const;

		template<typename T>
		const bool HasComponent() const;

		template<typename T>
		void RemoveComponent();

		UniqueIdentifier GetUniqueIdentifier() const { return GetComponent<IDComponent>().mUniqueIdentifier; }
		operator bool() const { return mEntityHandle != entt::null; }
		operator entt::entity() const { return mEntityHandle; }
		operator std::uint32_t() const { return static_cast<std::uint32_t>(mEntityHandle); }

		const std::string& GetName() const { return GetComponent<TagComponent>().mTag; }

		bool operator==(const Entity& aOther) const;

		bool operator!=(const Entity& aOther) const
		{
			return !(*this == aOther);
		}

	private:
		entt::entity mEntityHandle{entt::null};
		Scene* mScene = nullptr;
	};
}
