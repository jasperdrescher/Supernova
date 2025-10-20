#include "Timer.hpp"

#include "Time.hpp"

#include <chrono>

namespace Time
{
	void Timer::StartTimer()
	{
		mStartPoint = std::chrono::steady_clock::now();
	}

	void Timer::EndTimer()
	{
		mEndPoint = std::chrono::steady_clock::now();
	}

	double Timer::GetDurationMicroseconds() const
	{
		return Time::GetDurationMicroseconds(mEndPoint, mStartPoint);
	}

	double Timer::GetDurationMilliseconds() const
	{
		return Time::GetDurationMilliseconds(mEndPoint, mStartPoint);
	}

	double Timer::GetDurationSeconds() const
	{
		return Time::GetDurationSeconds(mEndPoint, mStartPoint);
	}

	const std::chrono::steady_clock::time_point& Timer::GetEndTime() const
	{
		return mEndPoint;
	}
}
