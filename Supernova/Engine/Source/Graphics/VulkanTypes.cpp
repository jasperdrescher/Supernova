#include "VulkanTypes.hpp"

#include "Core/Types.hpp"
#include "FileLoader.hpp"
#include "VulkanDevice.hpp"
#include "VulkanInitializers.hpp"
#include "VulkanTools.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <iostream>
#include <ktx.h>
#include <stdexcept>
#include <string>
#include <vector>

/**
* Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
*
* @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
* @param offset (Optional) Byte offset from beginning
*
* @return VkResult of the buffer mapping call
*/
VkResult Buffer::Map(VkDeviceSize aSize, VkDeviceSize aOffset)
{
	return vkMapMemory(mLogicalVkDevice, mVkDeviceMemory, aOffset, aSize, 0, &mMappedData);
}

/**
* Unmap a mapped memory range
*
* @note Does not return a result as vkUnmapMemory can't fail
*/
void Buffer::Unmap()
{
	if (mMappedData)
	{
		vkUnmapMemory(mLogicalVkDevice, mVkDeviceMemory);
		mMappedData = nullptr;
	}
}

/**
* Attach the allocated memory block to the buffer
*
* @param offset (Optional) Byte offset (from the beginning) for the memory region to bind
*
* @return VkResult of the bindBufferMemory call
*/
VkResult Buffer::Bind(VkDeviceSize aOffset)
{
	return vkBindBufferMemory(mLogicalVkDevice, mVkBuffer, mVkDeviceMemory, aOffset);
}

/**
* Setup the default descriptor for this buffer
*
* @param size (Optional) Size of the memory range of the descriptor
* @param offset (Optional) Byte offset from beginning
*
*/
void Buffer::SetupDescriptor(VkDeviceSize aSize, VkDeviceSize aOffset)
{
	mVkDescriptorBufferInfo.offset = aOffset;
	mVkDescriptorBufferInfo.buffer = mVkBuffer;
	mVkDescriptorBufferInfo.range = aSize;
}

/**
* Copies the specified data to the mapped buffer
*
* @param data Pointer to the data to copy
* @param size Size of the data to copy in machine units
*
*/
void Buffer::CopyTo(void* aData, VkDeviceSize aSize) const
{
	if (!mMappedData)
	{
		throw std::runtime_error("Invalid mapped data");
	}

	std::memcpy(mMappedData, aData, aSize);
}

/**
* Flush a memory range of the buffer to make it visible to the device
*
* @note Only required for non-coherent memory
*
* @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
* @param offset (Optional) Byte offset from beginning
*
* @return VkResult of the flush call
*/
VkResult Buffer::Flush(VkDeviceSize aSize, VkDeviceSize aOffset) const
{
	const VkMappedMemoryRange mappedMemoryRange{
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = mVkDeviceMemory,
		.offset = aOffset,
		.size = aSize
	};
	return vkFlushMappedMemoryRanges(mLogicalVkDevice, 1, &mappedMemoryRange);
}

/**
* Invalidate a memory range of the buffer to make it visible to the host
*
* @note Only required for non-coherent memory
*
* @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
* @param offset (Optional) Byte offset from beginning
*
* @return VkResult of the invalidate call
*/
VkResult Buffer::Invalidate(VkDeviceSize aSize, VkDeviceSize aOffset) const
{
	const VkMappedMemoryRange mappedMemoryRange{
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = mVkDeviceMemory,
		.offset = aOffset,
		.size = aSize
	};
	return vkInvalidateMappedMemoryRanges(mLogicalVkDevice, 1, &mappedMemoryRange);
}

/**
* Release all Vulkan resources held by this buffer
*/
void Buffer::Destroy()
{
	if (mVkBuffer)
	{
		vkDestroyBuffer(mLogicalVkDevice, mVkBuffer, nullptr);
		mVkBuffer = VK_NULL_HANDLE;
	}
	if (mVkDeviceMemory)
	{
		vkFreeMemory(mLogicalVkDevice, mVkDeviceMemory, nullptr);
		mVkDeviceMemory = VK_NULL_HANDLE;
	}
}

void ViewFrustum::UpdateFrustum(const Math::Matrix4f& aMatrix)
{
	mPlanes[static_cast<std::size_t>(Side::LEFT)].x = aMatrix[0].w + aMatrix[0].x;
	mPlanes[static_cast<std::size_t>(Side::LEFT)].y = aMatrix[1].w + aMatrix[1].x;
	mPlanes[static_cast<std::size_t>(Side::LEFT)].z = aMatrix[2].w + aMatrix[2].x;
	mPlanes[static_cast<std::size_t>(Side::LEFT)].w = aMatrix[3].w + aMatrix[3].x;

	mPlanes[static_cast<std::size_t>(Side::RIGHT)].x = aMatrix[0].w - aMatrix[0].x;
	mPlanes[static_cast<std::size_t>(Side::RIGHT)].y = aMatrix[1].w - aMatrix[1].x;
	mPlanes[static_cast<std::size_t>(Side::RIGHT)].z = aMatrix[2].w - aMatrix[2].x;
	mPlanes[static_cast<std::size_t>(Side::RIGHT)].w = aMatrix[3].w - aMatrix[3].x;

	mPlanes[static_cast<std::size_t>(Side::TOP)].x = aMatrix[0].w - aMatrix[0].y;
	mPlanes[static_cast<std::size_t>(Side::TOP)].y = aMatrix[1].w - aMatrix[1].y;
	mPlanes[static_cast<std::size_t>(Side::TOP)].z = aMatrix[2].w - aMatrix[2].y;
	mPlanes[static_cast<std::size_t>(Side::TOP)].w = aMatrix[3].w - aMatrix[3].y;

	mPlanes[static_cast<std::size_t>(Side::BOTTOM)].x = aMatrix[0].w + aMatrix[0].y;
	mPlanes[static_cast<std::size_t>(Side::BOTTOM)].y = aMatrix[1].w + aMatrix[1].y;
	mPlanes[static_cast<std::size_t>(Side::BOTTOM)].z = aMatrix[2].w + aMatrix[2].y;
	mPlanes[static_cast<std::size_t>(Side::BOTTOM)].w = aMatrix[3].w + aMatrix[3].y;

	mPlanes[static_cast<std::size_t>(Side::BACK)].x = aMatrix[0].w + aMatrix[0].z;
	mPlanes[static_cast<std::size_t>(Side::BACK)].y = aMatrix[1].w + aMatrix[1].z;
	mPlanes[static_cast<std::size_t>(Side::BACK)].z = aMatrix[2].w + aMatrix[2].z;
	mPlanes[static_cast<std::size_t>(Side::BACK)].w = aMatrix[3].w + aMatrix[3].z;

	mPlanes[static_cast<std::size_t>(Side::FRONT)].x = aMatrix[0].w - aMatrix[0].z;
	mPlanes[static_cast<std::size_t>(Side::FRONT)].y = aMatrix[1].w - aMatrix[1].z;
	mPlanes[static_cast<std::size_t>(Side::FRONT)].z = aMatrix[2].w - aMatrix[2].z;
	mPlanes[static_cast<std::size_t>(Side::FRONT)].w = aMatrix[3].w - aMatrix[3].z;

	for (std::size_t i = 0; i < mPlanes.size(); i++)
	{
		float length = std::sqrtf(mPlanes[i].x * mPlanes[i].x + mPlanes[i].y * mPlanes[i].y + mPlanes[i].z * mPlanes[i].z);
		mPlanes[i] /= length;
	}
}

bool ViewFrustum::IsInSphere(const Math::Vector3f& aPosition, float aRadius) const
{
	for (std::size_t i = 0; i < mPlanes.size(); i++)
	{
		if ((mPlanes[i].x * aPosition.x) + (mPlanes[i].y * aPosition.y) + (mPlanes[i].z * aPosition.z) + mPlanes[i].w <= -aRadius)
		{
			return false;
		}
	}
	return true;
}
