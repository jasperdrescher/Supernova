#pragma once

#include <vector>

class Sampler
{
public:
	explicit Sampler(const std::size_t aCapacity) : mData(aCapacity, 0.f), mCapacity(aCapacity)
	{
	}

	void Record(const float aValue)
	{
		if (mSize < mCapacity)
		{
			// Still filling the buffer
			mData[mIndex] = aValue;
			mSum += aValue;
			++mSize;
		}
		else
		{
			// Buffer is full: subtract the overwritten value and add the new one
			mSum = mSum - mData[mIndex] + aValue;
			mData[mIndex] = aValue;
		}

		// Advance index in circular fashion
		mIndex = (mIndex + 1u) % mCapacity;
	}

	[[nodiscard]] inline double GetAverage() const
	{
		return mSize == 0u ? 0.0 : static_cast<double>(mSum) / static_cast<double>(mSize);
	}

	[[nodiscard]] inline std::size_t Size() const
	{
		return mSize;
	}

	[[nodiscard]] inline const float* Data() const
	{
		return mData.data();
	}

	void Clear()
	{
		for (float& x : mData)
			x = 0.0f;

		mSize = 0u;
		mIndex = 0u;
		mSum = 0.0f;
	}
	
	void WriteSamplesInOrder(float* aTarget) const
	{
		if (mSize < mCapacity)
		{
			// Buffer not full: copy the valid samples (indices `0` .. `m_size - 1`)
			for (std::size_t i = 0u; i < mSize; ++i)
				aTarget[i] = mData[i];

			// Fill the rest with zeros
			for (std::size_t i = mSize; i < mCapacity; ++i)
				aTarget[i] = 0.f;
		}
		else
		{
			// Buffer is full: samples are stored in circular order
			// The oldest sample is at `m_data[m_index]`
			std::size_t pos = 0u;

			// Copy from `m_index` to the end
			for (std::size_t i = mIndex; i < mCapacity; ++i)
				aTarget[pos++] = mData[i];

			// Then copy from the beginning up to `m_index - 1`
			for (std::size_t i = 0u; i < mIndex; ++i)
				aTarget[pos++] = mData[i];
		}
	}

private:
	std::vector<float> mData;
	const std::size_t mCapacity;
	std::size_t mSize = 0; // Number of valid samples currently in the buffer
	std::size_t mIndex = 0; // Next index for insertion
	float mSum = 0.0f; // Running sum for fast averaging
};
