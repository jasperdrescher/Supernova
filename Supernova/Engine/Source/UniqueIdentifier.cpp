#include "UniqueIdentifier.hpp"

#include <cstdint>
#include <random>

static std::random_device gRandomDevice;
static std::mt19937_64 gRandomSeed(gRandomDevice());
static std::uniform_int_distribution<std::uint64_t> gUniformDistribution;

UniqueIdentifier::UniqueIdentifier()
	: mUniqueIdentifier(gUniformDistribution(gRandomSeed))
{
}

UniqueIdentifier::UniqueIdentifier(std::uint64_t aUniqueIdentifier)
	: mUniqueIdentifier(aUniqueIdentifier)
{
}
