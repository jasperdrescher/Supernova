#pragma once

#include "vulkan/vulkan_core.h"

#include <cstdint>
#include <string>
#include <vector>

namespace vks
{
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

			uint32_t graphics;
			uint32_t compute;
			uint32_t transfer;
		};

		/** @brief Physical device representation */
		VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};;
		/** @brief Logical device representation (application's view of the device) */
		VkDevice logicalDevice{VK_NULL_HANDLE};
		/** @brief Properties of the physical device including limits that the application can check against */
		VkPhysicalDeviceProperties properties{};
		/** @brief Features of the physical device that an application can use to check if a feature is supported */
		VkPhysicalDeviceFeatures features{};
		/** @brief Features that have been enabled for use on the physical device */
		VkPhysicalDeviceFeatures enabledFeatures{};
		/** @brief Memory types and heaps of the physical device */
		VkPhysicalDeviceMemoryProperties memoryProperties{};
		/** @brief Queue family properties of the physical device */
		std::vector<VkQueueFamilyProperties> queueFamilyProperties{};
		/** @brief List of extensions supported by the device */
		std::vector<std::string> supportedExtensions{};
		/** @brief Default command pool for the graphics queue family index */
		VkCommandPool commandPool{VK_NULL_HANDLE};;
		/** @brief Contains queue family indices */
		QueueFamilyIndices queueFamilyIndices;

		explicit VulkanDevice(VkPhysicalDevice aPhysicalDevice);
		~VulkanDevice();

		operator VkDevice() const
		{
			return logicalDevice;
		};

		uint32_t        getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr) const;
		uint32_t        getQueueFamilyIndex(VkQueueFlags queueFlags) const;
		VkResult        createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
		VkCommandPool   createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		bool            extensionSupported(std::string extension);
		VkFormat        getSupportedDepthFormat(bool checkSamplingSupport);
	};
}
