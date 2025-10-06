#include "VulkanTypes.hpp"

#include <cassert>
#include <cstring>

/**
* Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
*
* @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
* @param offset (Optional) Byte offset from beginning
*
* @return VkResult of the buffer mapping call
*/
VkResult VulkanBuffer::Map(VkDeviceSize aSize, VkDeviceSize aOffset)
{
	return vkMapMemory(mLogicalVkDevice, mVkDeviceMemory, aOffset, aSize, 0, &mMappedData);
}

/**
* Unmap a mapped memory range
*
* @note Does not return a result as vkUnmapMemory can't fail
*/
void VulkanBuffer::Unmap()
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
VkResult VulkanBuffer::Bind(VkDeviceSize aOffset)
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
void VulkanBuffer::SetupDescriptor(VkDeviceSize aSize, VkDeviceSize aOffset)
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
void VulkanBuffer::CopyTo(void* aData, VkDeviceSize aSize) const
{
	assert(mMappedData);
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
VkResult VulkanBuffer::Flush(VkDeviceSize aSize, VkDeviceSize aOffset) const
{
	VkMappedMemoryRange mappedRange{
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = mVkDeviceMemory,
		.offset = aOffset,
		.size = aSize
	};
	return vkFlushMappedMemoryRanges(mLogicalVkDevice, 1, &mappedRange);
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
VkResult VulkanBuffer::Invalidate(VkDeviceSize aSize, VkDeviceSize aOffset) const
{
	VkMappedMemoryRange mappedRange{
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = mVkDeviceMemory,
		.offset = aOffset,
		.size = aSize
	};
	return vkInvalidateMappedMemoryRanges(mLogicalVkDevice, 1, &mappedRange);
}

/**
* Release all Vulkan resources held by this buffer
*/
void VulkanBuffer::Destroy()
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
