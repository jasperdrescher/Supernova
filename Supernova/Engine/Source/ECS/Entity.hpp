#pragma once

#include "UniqueIdentifier.hpp"

#include <cstdint>
#include <string>

namespace entt
{
	using id_type = std::uint32_t;
	enum class entity : id_type;
}

namespace ECS
{
	class Scene;

	class Entity
	{
	public:
		Entity();
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

		UniqueIdentifier GetUniqueIdentifier() const;
		operator bool() const;
		operator entt::entity() const { return mEntityHandle; }
		operator std::uint32_t() const;

		const std::string& GetName() const;

		bool operator==(const Entity& aOther) const;

		bool operator!=(const Entity& aOther) const
		{
			return !(*this == aOther);
		}

	private:
		entt::entity mEntityHandle;
		Scene* mScene;
	};
}
