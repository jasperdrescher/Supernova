#pragma once

#include "vulkan/vulkan_core.h"

#include <cstdint>

namespace VulkanInitializers
{
	inline VkCommandBufferAllocateInfo CommandBufferAllocateInfo(
		VkCommandPool aVkCommandPool,
		VkCommandBufferLevel aVkCommandBufferLevel,
		std::uint32_t aBufferCount)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = aVkCommandPool;
		commandBufferAllocateInfo.level = aVkCommandBufferLevel;
		commandBufferAllocateInfo.commandBufferCount = aBufferCount;
		return commandBufferAllocateInfo;
	}

	inline VkCommandBufferBeginInfo CommandBufferBeginInfo()
	{
		VkCommandBufferBeginInfo cmdBufferBeginInfo{};
		cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		return cmdBufferBeginInfo;
	}

	inline VkImageMemoryBarrier ImageMemoryBarrier()
	{
		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		return imageMemoryBarrier;
	}
}
