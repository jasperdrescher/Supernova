#pragma once

#include "Core/Types.hpp"

class UniqueIdentifier
{
public:
	UniqueIdentifier();
	UniqueIdentifier(Core::uint64 aUniqueIdentifier);
	UniqueIdentifier(const UniqueIdentifier&) = default;

	operator Core::uint64() const { return mUniqueIdentifier; }

private:
	Core::uint64 mUniqueIdentifier;
};

namespace std
{
	template <typename T> struct hash;

	template<>
	struct hash<UniqueIdentifier>
	{
		Core::size operator()(const UniqueIdentifier& aUniqueIdentifier) const
		{
			return static_cast<Core::uint64>(aUniqueIdentifier);
		}
	};
}
