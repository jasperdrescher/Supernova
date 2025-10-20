#pragma once

#include <chrono>

namespace Time
{
	struct Timer
	{
		void StartTimer();
		void EndTimer();

		double GetDurationMicroseconds() const;
		double GetDurationMilliseconds() const;
		double GetDurationSeconds() const;
		const std::chrono::steady_clock::time_point& GetEndTime() const;

	private:
		std::chrono::steady_clock::time_point mStartPoint;
		std::chrono::steady_clock::time_point mEndPoint;
	};
}
