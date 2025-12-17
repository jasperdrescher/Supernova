#pragma once

#ifdef VULKAN_C

#include "Core/Types.hpp"
#include "VulkanCTypes.hpp"

#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

struct VulkanCDevice
{
	struct QueueFamilyIndices
	{
		QueueFamilyIndices()
			: mGraphics{0}
			, mCompute{0}
			, mTransfer{0}
		{
		}

		Core::uint32 mGraphics;
		Core::uint32 mCompute;
		Core::uint32 mTransfer;
	};

	VulkanCDevice();
	~VulkanCDevice();

	void CreateLogicalDevice(const std::vector<const char*>& aEnabledExtensions, void* aNextChain, bool aUseSwapChain = true, VkQueueFlags aRequestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
	void CreatePhysicalDevice(VkPhysicalDevice aVkPhysicalDevice);
	void FlushCommandBuffer(VkCommandBuffer aCommandBuffer, VkQueue aQueue, VkCommandPool aPool, bool aIsFree = true) const;
	void FlushCommandBuffer(VkCommandBuffer aCommandBuffer, VkQueue aQueue, bool aIsFree = true) const;
	void CopyBuffer(VulkanCTypes::Buffer* aSource, VulkanCTypes::Buffer* aDestination, VkQueue aQueue, VkBufferCopy* aCopyRegion = nullptr) const;

	VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel aLevel, VkCommandPool aPool, bool aIsBeginBuffer = false) const;
	VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel aLevel, bool aIsBeginBuffer = false) const;
	VkResult CreateBuffer(VkBufferUsageFlags aUsageFlags, VkMemoryPropertyFlags aMemoryPropertyFlags, VkDeviceSize aSize, VkBuffer* aBuffer, VkDeviceMemory* aMemory, void* aData = nullptr);
	VkResult CreateBuffer(VkBufferUsageFlags aUsageFlags, VkMemoryPropertyFlags aMemoryPropertyFlags, VulkanCTypes::Buffer* aBuffer, VkDeviceSize aSize, void* aData = nullptr) const;

	Core::uint32 GetMemoryTypeIndex(Core::uint32 aTypeBits, VkMemoryPropertyFlags aProperties, VkBool32* aMemTypeFound = nullptr) const;
	Core::uint32 GetQueueFamilyIndex(VkQueueFlags aVkQueueFlags) const;
	bool IsExtensionSupported(const std::string& aExtension) const;
	VkFormat GetSupportedDepthFormat(bool aCheckSamplingSupport) const;

	VkPhysicalDevice mPhysicalDevice;
	VkDevice mLogicalVkDevice;
	VkPhysicalDeviceProperties mPhysicalDeviceProperties{};
	VkPhysicalDeviceFeatures mPhysicalDeviceFeatures{};
	VkPhysicalDeviceFeatures mEnabledPhysicalDeviceFeatures{};
	VkPhysicalDeviceMemoryProperties mPhysicalDeviceMemoryProperties{};
	VkCommandPool mDefaultGraphicsCommandPool;
	std::vector<VkQueueFamilyProperties> mQueueFamilyProperties{};
	std::vector<std::string> mSupportedExtensions{};
	QueueFamilyIndices mQueueFamilyIndices;
};
#endif
