#pragma once

#include "vulkan/vulkan_core.h"

#include <cstdint>
#include <string>
#include <vector>

struct VulkanDevice
{
	struct QueueFamilyIndices
	{
		QueueFamilyIndices()
			: mGraphics{0}
			, mCompute{0}
			, mTransfer{0}
		{
		}

		std::uint32_t mGraphics;
		std::uint32_t mCompute;
		std::uint32_t mTransfer;
	};

	VulkanDevice();
	~VulkanDevice();

	void CreateLogicalDevice(std::vector<const char*> aEnabledExtensions, void* aNextChain, bool aUseSwapChain = true, VkQueueFlags aRequestedQueueTypes = VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT);
	void CreatePhysicalDevice(VkPhysicalDevice aVkPhysicalDevice);

	std::uint32_t GetMemoryTypeIndex(std::uint32_t aTypeBits, VkMemoryPropertyFlags aProperties, VkBool32 * aMemTypeFound = nullptr) const;
	std::uint32_t GetQueueFamilyIndex(VkQueueFlags aVkQueueFlags) const;
	bool IsExtensionSupported(const std::string& aExtension) const;
	VkFormat GetSupportedDepthFormat(bool aCheckSamplingSupport) const;

	/** @brief Physical device representation */
	VkPhysicalDevice mVkPhysicalDevice;

	/** @brief Logical device representation (application's view of the device) */
	VkDevice mLogicalVkDevice;

	/** @brief Properties of the physical device including limits that the application can check against */
	VkPhysicalDeviceProperties mVkPhysicalDeviceProperties{};

	/** @brief Features of the physical device that an application can use to check if a feature is supported */
	VkPhysicalDeviceFeatures mVkPhysicalDeviceFeatures{};

	/** @brief Features that have been enabled for use on the physical device */
	VkPhysicalDeviceFeatures mEnabledVkPhysicalDeviceFeatures{};

	/** @brief Memory types and heaps of the physical device */
	VkPhysicalDeviceMemoryProperties mVkPhysicalDeviceMemoryProperties{};

	/** @brief Queue family properties of the physical device */
	std::vector<VkQueueFamilyProperties> mVkQueueFamilyProperties{};

	/** @brief List of extensions supported by the device */
	std::vector<std::string> mSupportedExtensions{};

	/** @brief Contains queue family indices */
	QueueFamilyIndices mQueueFamilyIndices;
};
