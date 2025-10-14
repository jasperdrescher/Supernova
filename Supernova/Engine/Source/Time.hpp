#pragma once

#include <chrono>

namespace Time
{
	using TimePoint = std::chrono::steady_clock::time_point;

	double GetDurationMilliseconds(const TimePoint& aTimeA, const TimePoint& aTimeB);
	double GetDurationSeconds(const TimePoint& aTimeA, const TimePoint& aTimeB);
}
