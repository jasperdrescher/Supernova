#include "VulkanDevice.hpp"

#include "VulkanInitializers.hpp"
#include "VulkanTools.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

VulkanDevice::VulkanDevice()
	: mVkPhysicalDevice{VK_NULL_HANDLE}
	, mLogicalVkDevice{VK_NULL_HANDLE}
{
}

VulkanDevice::~VulkanDevice()
{
	if (mGraphicsVkCommandPool)
		vkDestroyCommandPool(mLogicalVkDevice, mGraphicsVkCommandPool, nullptr);

	if (mLogicalVkDevice != VK_NULL_HANDLE)
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
std::uint32_t VulkanDevice::GetMemoryTypeIndex(std::uint32_t aTypeBits, VkMemoryPropertyFlags aProperties, VkBool32* aMemTypeFound) const
{
	for (std::uint32_t i = 0; i < mVkPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
	{
		if ((aTypeBits & 1) == 1)
		{
			if ((mVkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & aProperties) == aProperties)
			{
				if (aMemTypeFound)
				{
					*aMemTypeFound = true;
				}
				return i;
			}
		}

		aTypeBits >>= 1;
	}

	if (aMemTypeFound)
	{
		*aMemTypeFound = false;
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
std::uint32_t VulkanDevice::GetQueueFamilyIndex(VkQueueFlags aVkQueueFlags) const
{
	// Dedicated queue for compute
	// Try to find a queue family index that supports compute but not graphics
	if ((aVkQueueFlags & VK_QUEUE_COMPUTE_BIT) == aVkQueueFlags)
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
	if ((aVkQueueFlags & VK_QUEUE_TRANSFER_BIT) == aVkQueueFlags)
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
		if ((mVkQueueFamilyProperties[i].queueFlags & aVkQueueFlags) == aVkQueueFlags)
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
void VulkanDevice::CreateLogicalDevice(std::vector<const char*> aEnabledExtensions, void* aNextChain, bool aUseSwapChain, VkQueueFlags aRequestedQueueTypes)
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

	// Get queue family indices for the requested queue family types
	// Note that the indices may overlap depending on the implementation

	const float defaultQueuePriority(0.0f);

	// Graphics queue
	if (aRequestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
	{
		mQueueFamilyIndices.mGraphics = GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = mQueueFamilyIndices.mGraphics;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &defaultQueuePriority;
		queueCreateInfos.push_back(queueInfo);
	}
	else
	{
		mQueueFamilyIndices.mGraphics = 0;
	}

	// Dedicated compute queue
	if (aRequestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
	{
		mQueueFamilyIndices.mCompute = GetQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
		if (mQueueFamilyIndices.mCompute != mQueueFamilyIndices.mGraphics)
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = mQueueFamilyIndices.mCompute;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		mQueueFamilyIndices.mCompute = mQueueFamilyIndices.mGraphics;
	}

	// Dedicated transfer queue
	if (aRequestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
	{
		mQueueFamilyIndices.mTransfer = GetQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
		if ((mQueueFamilyIndices.mTransfer != mQueueFamilyIndices.mGraphics) && (mQueueFamilyIndices.mTransfer != mQueueFamilyIndices.mCompute))
		{
			// If transfer family index differs, we need an additional queue create info for the transfer queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = mQueueFamilyIndices.mTransfer;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		mQueueFamilyIndices.mTransfer = mQueueFamilyIndices.mGraphics;
	}

	// Create the logical device representation
	std::vector<const char*> deviceExtensions(aEnabledExtensions);
	if (aUseSwapChain)
	{
		// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &mEnabledVkPhysicalDeviceFeatures;

	// If a pNext(Chain) has been passed, we need to add it to the device creation info
	VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
	if (aNextChain)
	{
		physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		physicalDeviceFeatures2.features = mEnabledVkPhysicalDeviceFeatures;
		physicalDeviceFeatures2.pNext = aNextChain;
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

		deviceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	}

	VK_CHECK_RESULT(vkCreateDevice(mVkPhysicalDevice, &deviceCreateInfo, nullptr, &mLogicalVkDevice));

	VkCommandPoolCreateInfo cmdPoolInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = mQueueFamilyIndices.mGraphics
	};
	VK_CHECK_RESULT(vkCreateCommandPool(mLogicalVkDevice, &cmdPoolInfo, nullptr, &mGraphicsVkCommandPool));
}

void VulkanDevice::CreatePhysicalDevice(VkPhysicalDevice aVkPhysicalDevice)
{
	mVkPhysicalDevice = aVkPhysicalDevice;

	// Store Properties features, limits and properties of the physical device for later use
	// Device properties also contain limits and sparse properties
	vkGetPhysicalDeviceProperties(aVkPhysicalDevice, &mVkPhysicalDeviceProperties);

	// Features should be checked by the examples before using them
	vkGetPhysicalDeviceFeatures(aVkPhysicalDevice, &mVkPhysicalDeviceFeatures);

	if (mVkPhysicalDeviceFeatures.samplerAnisotropy)
	{
		mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy = VK_TRUE;
	}

	// Memory properties are used regularly for creating all kinds of buffers
	vkGetPhysicalDeviceMemoryProperties(aVkPhysicalDevice, &mVkPhysicalDeviceMemoryProperties);

	// Queue family properties, used for setting up requested queues upon device creation
	std::uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(aVkPhysicalDevice, &queueFamilyCount, nullptr);
	assert(queueFamilyCount > 0);

	mVkQueueFamilyProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(aVkPhysicalDevice, &queueFamilyCount, mVkQueueFamilyProperties.data());

	std::uint32_t extensionCount = 0;
	VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(aVkPhysicalDevice, nullptr, &extensionCount, nullptr));
	if (extensionCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extensionCount);
		if (vkEnumerateDeviceExtensionProperties(aVkPhysicalDevice, nullptr, &extensionCount, &extensions.front()) == VK_SUCCESS)
		{
			for (const VkExtensionProperties& vkExtensionProperties : extensions)
			{
				mSupportedExtensions.push_back(vkExtensionProperties.extensionName);
			}
		}
	}

	if (mVkPhysicalDeviceProperties.apiVersion < VK_API_VERSION_1_3)
	{
		throw std::runtime_error(std::format("Selected GPU does not support support Vulkan 1.3: {}", VulkanTools::GetErrorString(VK_ERROR_INCOMPATIBLE_DRIVER)));
	}
}

VkCommandBuffer VulkanDevice::CreateCommandBuffer(VkCommandBufferLevel aLevel, VkCommandPool aPool, bool aIsBeginBuffer) const
{
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = VulkanInitializers::commandBufferAllocateInfo(aPool, aLevel, 1);
	VkCommandBuffer cmdBuffer;
	VK_CHECK_RESULT(vkAllocateCommandBuffers(mLogicalVkDevice, &cmdBufAllocateInfo, &cmdBuffer));
	// If requested, also start recording for the new command buffer
	if (aIsBeginBuffer)
	{
		VkCommandBufferBeginInfo cmdBufInfo = VulkanInitializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}
	return cmdBuffer;
}

VkCommandBuffer VulkanDevice::CreateCommandBuffer(VkCommandBufferLevel aLevel, bool aIsBeginBuffer) const
{
	return CreateCommandBuffer(aLevel, mGraphicsVkCommandPool, aIsBeginBuffer);
}

void VulkanDevice::FlushCommandBuffer(VkCommandBuffer aCommandBuffer, VkQueue aQueue, VkCommandPool aPool, bool aIsFree) const
{
	if (aCommandBuffer == VK_NULL_HANDLE)
		return;

	VK_CHECK_RESULT(vkEndCommandBuffer(aCommandBuffer));

	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &aCommandBuffer
	};
	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceInfo = VulkanInitializers::fenceCreateInfo(gVkFlagsNone);
	VkFence fence;
	VK_CHECK_RESULT(vkCreateFence(mLogicalVkDevice, &fenceInfo, nullptr, &fence));
	// Submit to the queue
	VK_CHECK_RESULT(vkQueueSubmit(aQueue, 1, &submitInfo, fence));
	// Wait for the fence to signal that command buffer has finished executing
	VK_CHECK_RESULT(vkWaitForFences(mLogicalVkDevice, 1, &fence, VK_TRUE, gDefaultFenceTimeoutNS));
	vkDestroyFence(mLogicalVkDevice, fence, nullptr);
	if (aIsFree)
	{
		vkFreeCommandBuffers(mLogicalVkDevice, aPool, 1, &aCommandBuffer);
	}
}

void VulkanDevice::flushCommandBuffer(VkCommandBuffer aCommandBuffer, VkQueue aQueue, bool aIsFree) const
{
	return FlushCommandBuffer(aCommandBuffer, aQueue, mGraphicsVkCommandPool, aIsFree);
}

/**
* Check if an extension is supported by the (physical device)
*
* @param extension Name of the extension to check
*
* @return True if the extension is supported (present in the list read at device creation time)
*/
bool VulkanDevice::IsExtensionSupported(const std::string& aExtension) const
{
	return (std::find(mSupportedExtensions.begin(), mSupportedExtensions.end(), aExtension) != mSupportedExtensions.end());
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
VkFormat VulkanDevice::GetSupportedDepthFormat(bool aCheckSamplingSupport) const
{
	// All depth formats may be optional, so we need to find a suitable depth format to use
	std::vector<VkFormat> depthFormats = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
	for (VkFormat& format : depthFormats)
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(mVkPhysicalDevice, format, &formatProperties);
		// Format must support depth stencil attachment for optimal tiling
		if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			if (aCheckSamplingSupport)
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

/**
	* Create a buffer on the device
	*
	* @param usageFlags Usage flag bit mask for the buffer (i.e. index, vertex, uniform buffer)
	* @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
	* @param size Size of the buffer in byes
	* @param buffer Pointer to the buffer handle acquired by the function
	* @param memory Pointer to the memory handle acquired by the function
	* @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
	*
	* @return VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
	*/
VkResult VulkanDevice::CreateBuffer(VkBufferUsageFlags aUsageFlags, VkMemoryPropertyFlags aMemoryPropertyFlags, VkDeviceSize aSize, VkBuffer* aBuffer, VkDeviceMemory* aMemory, void* aData)
{
	// Create the buffer handle
	VkBufferCreateInfo bufferCreateInfo = VulkanInitializers::bufferCreateInfo(aUsageFlags, aSize);
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK_RESULT(vkCreateBuffer(mLogicalVkDevice, &bufferCreateInfo, nullptr, aBuffer));

	// Create the memory backing up the buffer handle
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAlloc = VulkanInitializers::memoryAllocateInfo();
	vkGetBufferMemoryRequirements(mLogicalVkDevice, *aBuffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	// Find a memory type index that fits the properties of the buffer
	memAlloc.memoryTypeIndex = GetMemoryTypeIndex(memReqs.memoryTypeBits, aMemoryPropertyFlags);
	// If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
	if (aUsageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
		allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		memAlloc.pNext = &allocFlagsInfo;
	}
	VK_CHECK_RESULT(vkAllocateMemory(mLogicalVkDevice, &memAlloc, nullptr, aMemory));

	// If a pointer to the buffer data has been passed, map the buffer and copy over the data
	if (aData != nullptr)
	{
		void* mapped;
		VK_CHECK_RESULT(vkMapMemory(mLogicalVkDevice, *aMemory, 0, aSize, 0, &mapped));
		memcpy(mapped, aData, aSize);
		// If host coherency hasn't been requested, do a manual flush to make writes visible
		if ((aMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
		{
			VkMappedMemoryRange mappedRange{
				.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
				.memory = *aMemory,
				.size = aSize
			};
			vkFlushMappedMemoryRanges(mLogicalVkDevice, 1, &mappedRange);
		}
		vkUnmapMemory(mLogicalVkDevice, *aMemory);
	}

	// Attach the memory to the buffer object
	VK_CHECK_RESULT(vkBindBufferMemory(mLogicalVkDevice, *aBuffer, *aMemory, 0));

	return VK_SUCCESS;
}

VkResult VulkanDevice::CreateBuffer(VkBufferUsageFlags aUsageFlags, VkMemoryPropertyFlags aMemoryPropertyFlags, VulkanBuffer* aBuffer, VkDeviceSize aSize, void* aData) const
{
	aBuffer->mLogicalVkDevice = mLogicalVkDevice;

	// Create the buffer handle
	VkBufferCreateInfo bufferCreateInfo = VulkanInitializers::bufferCreateInfo(aUsageFlags, aSize);
	VK_CHECK_RESULT(vkCreateBuffer(mLogicalVkDevice, &bufferCreateInfo, nullptr, &aBuffer->mVkBuffer));

	// Create the memory backing up the buffer handle
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAlloc = VulkanInitializers::memoryAllocateInfo();
	vkGetBufferMemoryRequirements(mLogicalVkDevice, aBuffer->mVkBuffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	// Find a memory type index that fits the properties of the buffer
	memAlloc.memoryTypeIndex = GetMemoryTypeIndex(memReqs.memoryTypeBits, aMemoryPropertyFlags);
	// If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
	VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
	if (aUsageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	{
		allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
		allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
		memAlloc.pNext = &allocFlagsInfo;
	}
	VK_CHECK_RESULT(vkAllocateMemory(mLogicalVkDevice, &memAlloc, nullptr, &aBuffer->mVkDeviceMemory));

	aBuffer->mVkDeviceAlignment = memReqs.alignment;
	aBuffer->mVkDeviceSize = aSize;
	aBuffer->mUsageFlags = aUsageFlags;
	aBuffer->mMemoryPropertyFlags = aMemoryPropertyFlags;

	// If a pointer to the buffer data has been passed, map the buffer and copy over the data
	if (aData != nullptr)
	{
		VK_CHECK_RESULT(aBuffer->Map(VK_WHOLE_SIZE, 0));
		memcpy(aBuffer->mMappedData, aData, aSize);
		if ((aMemoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			aBuffer->Flush(VK_WHOLE_SIZE, 0);

		aBuffer->Unmap();
	}

	// Initialize a default descriptor that covers the whole buffer size
	aBuffer->SetupDescriptor(VK_WHOLE_SIZE, 0);

	// Attach the memory to the buffer object
	return aBuffer->Bind(0);
}
