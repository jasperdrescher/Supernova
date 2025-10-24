#include "UniqueIdentifier.hpp"

#include "Core/Types.hpp"

#include <random>

static std::random_device gRandomDevice;
static std::mt19937_64 gRandomSeed(gRandomDevice());
static std::uniform_int_distribution<Core::uint64> gUniformDistribution;

UniqueIdentifier::UniqueIdentifier()
	: mUniqueIdentifier(gUniformDistribution(gRandomSeed))
{
}

UniqueIdentifier::UniqueIdentifier(Core::uint64 aUniqueIdentifier)
	: mUniqueIdentifier(aUniqueIdentifier)
{
}
