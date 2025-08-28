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
			: graphics(0)
			, compute(0)
			, transfer(0)
		{
		}

		std::uint32_t graphics;
		std::uint32_t compute;
		std::uint32_t transfer;
	};

	VulkanDevice();
	~VulkanDevice();

	std::uint32_t        getMemoryType(std::uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr) const;
	std::uint32_t        getQueueFamilyIndex(VkQueueFlags queueFlags) const;
	VkResult        CreateLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
	void CreatePhysicalDevice(VkPhysicalDevice aVkPhysicalDevice);
	bool            IsExtensionSupported(std::string extension);
	VkFormat        GetSupportedDepthFormat(bool checkSamplingSupport) const;

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
