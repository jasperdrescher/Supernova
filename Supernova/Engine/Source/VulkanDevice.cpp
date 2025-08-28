#include "VulkanDevice.hpp"

#include "VulkanTools.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

VulkanDevice::VulkanDevice(VkPhysicalDevice aPhysicalDevice)
	: mVkPhysicalDevice{VK_NULL_HANDLE}
	, mLogicalVkDevice{VK_NULL_HANDLE}
	, mVkCommandPool{VK_NULL_HANDLE}
{
	assert(aPhysicalDevice);
	mVkPhysicalDevice = aPhysicalDevice;

	// Store Properties features, limits and properties of the physical device for later use
	// Device properties also contain limits and sparse properties
	vkGetPhysicalDeviceProperties(mVkPhysicalDevice, &mVkPhysicalDeviceProperties);
	// Features should be checked by the examples before using them
	vkGetPhysicalDeviceFeatures(mVkPhysicalDevice, &mVkPhysicalDeviceFeatures);
	// Memory properties are used regularly for creating all kinds of buffers
	vkGetPhysicalDeviceMemoryProperties(mVkPhysicalDevice, &VkPhysicalDeviceMemoryProperties);
	// Queue family properties, used for setting up requested queues upon device creation
	std::uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(mVkPhysicalDevice, &queueFamilyCount, nullptr);
	assert(queueFamilyCount > 0);
	mVkQueueFamilyProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(mVkPhysicalDevice, &queueFamilyCount, mVkQueueFamilyProperties.data());

	// Get list of supported extensions
	std::uint32_t extCount = 0;
	vkEnumerateDeviceExtensionProperties(mVkPhysicalDevice, nullptr, &extCount, nullptr);
	if (extCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extCount);
		if (vkEnumerateDeviceExtensionProperties(mVkPhysicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
		{
			for (auto& ext : extensions)
			{
				mSupportedExtensions.push_back(ext.extensionName);
			}
		}
	}
}

VulkanDevice::~VulkanDevice()
{
	if (mVkCommandPool)
		vkDestroyCommandPool(mLogicalVkDevice, mVkCommandPool, nullptr);

	if (mLogicalVkDevice)
		vkDestroyDevice(mLogicalVkDevice, nullptr);
}

/**
* Get the index of a memory type that has all the requested property bits set
*
* @param typeBits Bit mask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
* @param properties Bit mask of properties for the memory type to request
* @param (Optional) memTypeFound Pointer to a bool that is set to true if a matching memory type has been found
*
* @return Index of the requested memory type
*
* @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
*/
std::uint32_t VulkanDevice::getMemoryType(std::uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound) const
{
	for (std::uint32_t i = 0; i < VkPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((VkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				if (memTypeFound)
				{
					*memTypeFound = true;
				}
				return i;
			}
		}
		typeBits >>= 1;
	}

	if (memTypeFound)
	{
		*memTypeFound = false;
		return 0;
	}
	else
	{
		throw std::runtime_error("Could not find a matching memory type");
	}
}

/**
* Get the index of a queue family that supports the requested queue flags
* SRS - support VkQueueFlags parameter for requesting multiple flags vs. VkQueueFlagBits for a single flag only
*
* @param queueFlags Queue flags to find a queue family index for
*
* @return Index of the queue family index that matches the flags
*
* @throw Throws an exception if no queue family index could be found that supports the requested flags
*/
std::uint32_t VulkanDevice::getQueueFamilyIndex(VkQueueFlags queueFlags) const
{
	// Dedicated queue for compute
	// Try to find a queue family index that supports compute but not graphics
	if ((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags)
	{
		for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(mVkQueueFamilyProperties.size()); i++)
		{
			if ((mVkQueueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && ((mVkQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				return i;
			}
		}
	}

	// Dedicated queue for transfer
	// Try to find a queue family index that supports transfer but not graphics and compute
	if ((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags)
	{
		for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(mVkQueueFamilyProperties.size()); i++)
		{
			if ((mVkQueueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && ((mVkQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((mVkQueueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				return i;
			}
		}
	}

	// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
	for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(mVkQueueFamilyProperties.size()); i++)
	{
		if ((mVkQueueFamilyProperties[i].queueFlags & queueFlags) == queueFlags)
		{
			return i;
		}
	}

	throw std::runtime_error("Could not find a matching queue family index");
}

/**
* Create the logical device based on the assigned physical device, also gets default queue family indices
*
* @param enabledFeatures Can be used to enable certain features upon device creation
* @param pNextChain Optional chain of pointer to extension structures
* @param useSwapChain Set to false for headless rendering to omit the swapchain device extensions
* @param requestedQueueTypes Bit flags specifying the queue types to be requested from the device
*
* @return VkResult of the device creation call
*/
VkResult VulkanDevice::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain, VkQueueFlags requestedQueueTypes)
{
	// Desired queues need to be requested upon logical device creation
	// Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
	// requests different queue types

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

	// Get queue family indices for the requested queue family types
	// Note that the indices may overlap depending on the implementation

	const float defaultQueuePriority(0.0f);

	// Graphics queue
	if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
	{
		mQueueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = mQueueFamilyIndices.graphics;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &defaultQueuePriority;
		queueCreateInfos.push_back(queueInfo);
	}
	else
	{
		mQueueFamilyIndices.graphics = 0;
	}

	// Dedicated compute queue
	if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
	{
		mQueueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
		if (mQueueFamilyIndices.compute != mQueueFamilyIndices.graphics)
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = mQueueFamilyIndices.compute;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		mQueueFamilyIndices.compute = mQueueFamilyIndices.graphics;
	}

	// Dedicated transfer queue
	if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
	{
		mQueueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
		if ((mQueueFamilyIndices.transfer != mQueueFamilyIndices.graphics) && (mQueueFamilyIndices.transfer != mQueueFamilyIndices.compute))
		{
			// If transfer family index differs, we need an additional queue create info for the transfer queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = mQueueFamilyIndices.transfer;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		mQueueFamilyIndices.transfer = mQueueFamilyIndices.graphics;
	}

	// Create the logical device representation
	std::vector<const char*> deviceExtensions(enabledExtensions);
	if (useSwapChain)
	{
		// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

	// If a pNext(Chain) has been passed, we need to add it to the device creation info
	VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
	if (pNextChain)
	{
		physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		physicalDeviceFeatures2.features = enabledFeatures;
		physicalDeviceFeatures2.pNext = pNextChain;
		deviceCreateInfo.pEnabledFeatures = nullptr;
		deviceCreateInfo.pNext = &physicalDeviceFeatures2;
	}

	if (deviceExtensions.size() > 0)
	{
		for (const char* enabledExtension : deviceExtensions)
		{
			if (!IsExtensionSupported(enabledExtension))
			{
				std::cerr << "Enabled device extension \"" << enabledExtension << "\" is not present at device level\n";
			}
		}

		deviceCreateInfo.enabledExtensionCount = (std::uint32_t)deviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	}

	mEnabledVkPhysicalDeviceFeatures = enabledFeatures;

	VkResult result = vkCreateDevice(mVkPhysicalDevice, &deviceCreateInfo, nullptr, &mLogicalVkDevice);
	if (result != VK_SUCCESS)
	{
		return result;
	}

	// Create a default command pool for graphics command buffers
	mVkCommandPool = CreateCommandPool(mQueueFamilyIndices.graphics);

	return result;
}

/**
* Create a command pool for allocation command buffers from
*
* @param queueFamilyIndex Family index of the queue to create the command pool for
* @param createFlags (Optional) Command pool creation flags (Defaults to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
*
* @note Command buffers allocated from the created pool can only be submitted to a queue with the same family index
*
* @return A handle to the created command buffer
*/
VkCommandPool VulkanDevice::CreateCommandPool(std::uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags) const
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
	cmdPoolInfo.flags = createFlags;
	VkCommandPool cmdPool;
	VK_CHECK_RESULT(vkCreateCommandPool(mLogicalVkDevice, &cmdPoolInfo, nullptr, &cmdPool));
	return cmdPool;
}

/**
* Check if an extension is supported by the (physical device)
*
* @param extension Name of the extension to check
*
* @return True if the extension is supported (present in the list read at device creation time)
*/
bool VulkanDevice::IsExtensionSupported(std::string extension)
{
	return (std::find(mSupportedExtensions.begin(), mSupportedExtensions.end(), extension) != mSupportedExtensions.end());
}

/**
* Select the best-fit depth format for this device from a list of possible depth (and stencil) formats
*
* @param checkSamplingSupport Check if the format can be sampled from (e.g. for shader reads)
*
* @return The depth format that best fits for the current device
*
* @throw Throws an exception if no depth format fits the requirements
*/
VkFormat VulkanDevice::GetSupportedDepthFormat(bool checkSamplingSupport) const
{
	// All depth formats may be optional, so we need to find a suitable depth format to use
	std::vector<VkFormat> depthFormats = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
	for (auto& format : depthFormats)
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(mVkPhysicalDevice, format, &formatProperties);
		// Format must support depth stencil attachment for optimal tiling
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			if (checkSamplingSupport)
			{
				if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
				{
					continue;
				}
			}
			return format;
		}
	}
	throw std::runtime_error("Could not find a matching depth format");
}
