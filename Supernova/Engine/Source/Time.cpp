#include "Time.hpp"

#include <chrono>
#include <ratio>

namespace Time
{
	double GetDurationMilliseconds(const TimePoint& aTimeA, const TimePoint& aTimeB)
	{
		return std::chrono::duration<double, std::milli>(aTimeA - aTimeB).count();
	}

	double GetDurationSeconds(const TimePoint& aTimeA, const TimePoint& aTimeB)
	{
		return std::chrono::duration<double>(aTimeA - aTimeB).count();
	}
}
