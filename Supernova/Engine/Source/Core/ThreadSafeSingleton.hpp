#pragma once

#include <mutex>
#include <shared_mutex>

using WriteLock = std::unique_lock<std::shared_mutex>;
using ReadLock = std::shared_lock<std::shared_mutex>;

template <class T>
class ThreadSafeSingleton
{
public:
	static T& GetInstance()
	{
		static T instance; // The initialization of static local variables is guaranteed to be thread-safe
		return instance;
	}

protected:
	ThreadSafeSingleton() = default;
	~ThreadSafeSingleton() = default;

	[[nodiscard]] WriteLock AcquireWriteLock() const
	{
		return WriteLock(mMutex);
	}

	[[nodiscard]] ReadLock AcquireReadLock() const
	{
		return ReadLock(mMutex);
	}

private:
	ThreadSafeSingleton(const ThreadSafeSingleton&) = delete;
	ThreadSafeSingleton& operator=(const ThreadSafeSingleton&) = delete;

	mutable std::shared_mutex mMutex;
};
