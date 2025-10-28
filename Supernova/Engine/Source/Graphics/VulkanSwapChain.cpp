#include "VulkanSwapChain.hpp"

#include "Core/Constants.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTools.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <vector>

VulkanSwapChain::VulkanSwapChain()
	: mActiveVulkanDevice{nullptr}
	, mActiveVkInstance{VK_NULL_HANDLE}
	, mVkSurfaceKHR{VK_NULL_HANDLE}
	, mColorVkFormat{VK_FORMAT_UNDEFINED}
	, mVkColorSpaceKHR{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
	, mVkSwapchainKHR{VK_NULL_HANDLE}
	, mQueueNodeIndex{Core::uint32_max}
	, mImageCount{0}
{
}

void VulkanSwapChain::InitializeSurface()
{
	std::uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(mActiveVulkanDevice->mVkPhysicalDevice, &queueCount, nullptr);
	assert(queueCount >= 1);

	std::vector<VkQueueFamilyProperties> queueProps(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(mActiveVulkanDevice->mVkPhysicalDevice, &queueCount, queueProps.data());

	// Iterate over each queue to learn whether it supports presenting:
	// Find a queue with present support
	// Will be used to present the swap chain images to the windowing system
	std::vector<VkBool32> supportsPresent(queueCount);
	for (std::uint32_t i = 0; i < queueCount; i++)
	{
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(mActiveVulkanDevice->mVkPhysicalDevice, i, mVkSurfaceKHR, &supportsPresent[i]));
	}

	// Search for a graphics and a present queue in the array of queue
	// families, try to find one that supports both
	std::uint32_t graphicsQueueNodeIndex = Core::uint32_max;
	std::uint32_t presentQueueNodeIndex = Core::uint32_max;
	for (std::uint32_t i = 0; i < queueCount; i++)
	{
		if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			if (graphicsQueueNodeIndex == Core::uint32_max)
			{
				graphicsQueueNodeIndex = i;
			}

			if (supportsPresent[i] == VK_TRUE)
			{
				graphicsQueueNodeIndex = i;
				presentQueueNodeIndex = i;
				break;
			}
		}
	}
	if (presentQueueNodeIndex == Core::uint32_max)
	{
		// If there's no queue that supports both present and graphics
		// try to find a separate present queue
		for (std::uint32_t i = 0; i < queueCount; ++i)
		{
			if (supportsPresent[i] == VK_TRUE)
			{
				presentQueueNodeIndex = i;
				break;
			}
		}
	}

	// Exit if either a graphics or a presenting queue hasn't been found
	if (graphicsQueueNodeIndex == Core::uint32_max || presentQueueNodeIndex == Core::uint32_max)
	{
		throw std::runtime_error("Could not find a graphics and/or presenting queue!");
	}

	if (graphicsQueueNodeIndex != presentQueueNodeIndex)
	{
		throw std::runtime_error("Separate graphics and presenting queues are not supported yet!");
	}

	mQueueNodeIndex = graphicsQueueNodeIndex;

	// Get list of supported surface formats
	std::uint32_t formatCount;
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(mActiveVulkanDevice->mVkPhysicalDevice, mVkSurfaceKHR, &formatCount, nullptr));
	assert(formatCount > 0);

	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(mActiveVulkanDevice->mVkPhysicalDevice, mVkSurfaceKHR, &formatCount, surfaceFormats.data()));

	// We want to get a format that best suits our needs, so we try to get one from a set of preferred formats
	// Initialize the format to the first one returned by the implementation in case we can't find one of the preffered formats
	VkSurfaceFormatKHR selectedFormat = surfaceFormats[0];
	const std::vector<VkFormat> preferredImageFormats = {
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32
	};

	for (const VkSurfaceFormatKHR& availableFormat : surfaceFormats)
	{
		if (std::find(preferredImageFormats.begin(), preferredImageFormats.end(), availableFormat.format) != preferredImageFormats.end())
		{
			selectedFormat = availableFormat;
			break;
		}
	}

	mColorVkFormat = selectedFormat.format;
	mVkColorSpaceKHR = selectedFormat.colorSpace;
}

void VulkanSwapChain::SetContext(VkInstance aVkInstance, VulkanDevice* aVulkanDevice)
{
	mActiveVkInstance = aVkInstance;
	mActiveVulkanDevice = aVulkanDevice;
}

void VulkanSwapChain::CreateSwapchain(std::uint32_t& aWidth, std::uint32_t& aHeight, bool aUseVSync)
{
	assert(mActiveVulkanDevice);
	assert(mActiveVkInstance);

	// Store the current swap chain handle so we can use it later on to ease up recreation
	VkSwapchainKHR oldSwapchain = mVkSwapchainKHR;

	// Get physical device surface properties and formats
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mActiveVulkanDevice->mVkPhysicalDevice, mVkSurfaceKHR, &surfaceCapabilities));

	VkExtent2D swapchainExtent = {};
	// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
	if (surfaceCapabilities.currentExtent.width == static_cast<std::uint32_t>(-1))
	{
		// If the surface size is undefined, the size is set to the size of the images requested
		swapchainExtent.width = aWidth;
		swapchainExtent.height = aHeight;
	}
	else
	{
		// If the surface size is defined, the swap chain size must match
		swapchainExtent = surfaceCapabilities.currentExtent;
		aWidth = surfaceCapabilities.currentExtent.width;
		aHeight = surfaceCapabilities.currentExtent.height;
	}


	// Select a present mode for the swapchain
	std::uint32_t presentModeCount;
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(mActiveVulkanDevice->mVkPhysicalDevice, mVkSurfaceKHR, &presentModeCount, nullptr));
	assert(presentModeCount > 0);

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(mActiveVulkanDevice->mVkPhysicalDevice, mVkSurfaceKHR, &presentModeCount, presentModes.data()));

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
	if (!aUseVSync)
	{
		for (std::size_t i = 0; i < presentModeCount; i++)
		{
			if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}

			if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}
	}

	// Determine the number of images
	std::uint32_t desiredNumberOfSwapchainImages = surfaceCapabilities.minImageCount + 1;
	if ((surfaceCapabilities.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfaceCapabilities.maxImageCount))
	{
		desiredNumberOfSwapchainImages = surfaceCapabilities.maxImageCount;
	}

	// Find the transformation of the surface
	VkSurfaceTransformFlagsKHR preTransform;
	if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		// We prefer a non-rotated transform
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		preTransform = surfaceCapabilities.currentTransform;
	}

	// Find a supported composite alpha format (not all devices support alpha opaque)
	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	// Simply select the first composite alpha format available
	const std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};

	for (const VkCompositeAlphaFlagBitsKHR& compositeAlphaFlag : compositeAlphaFlags)
	{
		if (surfaceCapabilities.supportedCompositeAlpha & compositeAlphaFlag)
		{
			compositeAlpha = compositeAlphaFlag;
			break;
		};
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = mVkSurfaceKHR,
		.minImageCount = desiredNumberOfSwapchainImages,
		.imageFormat = mColorVkFormat,
		.imageColorSpace = mVkColorSpaceKHR,
		.imageExtent = {swapchainExtent.width, swapchainExtent.height},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform,
		.compositeAlpha = compositeAlpha,
		.presentMode = swapchainPresentMode, // Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
		.clipped = VK_TRUE, // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		.oldSwapchain = oldSwapchain,
	};

	// Enable transfer source on swap chain images if supported
	if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
	{
		swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	// Enable transfer destination on swap chain images if supported
	if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	{
		swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	VK_CHECK_RESULT(vkCreateSwapchainKHR(mActiveVulkanDevice->mLogicalVkDevice, &swapchainCreateInfo, nullptr, &mVkSwapchainKHR));

	// If an existing swap chain is re-created, destroy the old swap chain and the ressources owned by the application (image views, images are owned by the swap chain)
	if (oldSwapchain != VK_NULL_HANDLE)
	{
		for (std::size_t i = 0; i < mVkImages.size(); i++)
			vkDestroyImageView(mActiveVulkanDevice->mLogicalVkDevice, mVkImageViews[i], nullptr);
		
		vkDestroySwapchainKHR(mActiveVulkanDevice->mLogicalVkDevice, oldSwapchain, nullptr);
	}

	VK_CHECK_RESULT(vkGetSwapchainImagesKHR(mActiveVulkanDevice->mLogicalVkDevice, mVkSwapchainKHR, &mImageCount, nullptr));

	// Get the swap chain images
	mVkImages.resize(mImageCount);
	VK_CHECK_RESULT(vkGetSwapchainImagesKHR(mActiveVulkanDevice->mLogicalVkDevice, mVkSwapchainKHR, &mImageCount, mVkImages.data()));

	// Get the swap chain buffers containing the image and imageview
	mVkImageViews.resize(mImageCount);
	for (std::size_t i = 0; i < mVkImages.size(); i++)
	{
		const VkImageViewCreateInfo colorAttachmentViewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = mVkImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = mColorVkFormat,
			.components = {
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_A
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};
		VK_CHECK_RESULT(vkCreateImageView(mActiveVulkanDevice->mLogicalVkDevice, &colorAttachmentViewCreateInfo, nullptr, &mVkImageViews[i]));
	}
}

VkResult VulkanSwapChain::AcquireNextImage(VkSemaphore aPresentCompleteSemaphore, std::uint32_t& aImageIndex) const
{
	// By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
	// With that we don't have to handle VK_NOT_READY
	return vkAcquireNextImageKHR(mActiveVulkanDevice->mLogicalVkDevice, mVkSwapchainKHR, UINT64_MAX, aPresentCompleteSemaphore, static_cast<VkFence>(VK_NULL_HANDLE), &aImageIndex);
}

void VulkanSwapChain::CleanUp()
{
	if (mVkSwapchainKHR != VK_NULL_HANDLE)
	{
		for (std::size_t i = 0; i < mVkImages.size(); i++)
			vkDestroyImageView(mActiveVulkanDevice->mLogicalVkDevice, mVkImageViews[i], nullptr);

		vkDestroySwapchainKHR(mActiveVulkanDevice->mLogicalVkDevice, mVkSwapchainKHR, nullptr);
	}

	if (mVkSurfaceKHR != VK_NULL_HANDLE)
		vkDestroySurfaceKHR(mActiveVkInstance, mVkSurfaceKHR, nullptr);
	
	mVkSurfaceKHR = VK_NULL_HANDLE;
	mVkSwapchainKHR = VK_NULL_HANDLE;
}
