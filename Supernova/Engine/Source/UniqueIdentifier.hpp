#pragma once

#include <cstdint>

class UniqueIdentifier
{
public:
	UniqueIdentifier();
	UniqueIdentifier(std::uint64_t aUniqueIdentifier);
	UniqueIdentifier(const UniqueIdentifier&) = default;

	operator std::uint64_t() const { return mUniqueIdentifier; }

private:
	std::uint64_t mUniqueIdentifier;
};

namespace std
{
	template <typename T> struct hash;

	template<>
	struct hash<UniqueIdentifier>
	{
		size_t operator()(const UniqueIdentifier& aUniqueIdentifier) const
		{
			return static_cast<uint64_t>(aUniqueIdentifier);
		}
	};
}
