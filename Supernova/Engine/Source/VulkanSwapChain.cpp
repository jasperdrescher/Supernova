#include "VulkanSwapChain.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTools.hpp"

#include <algorithm>
#include <cassert>

VulkanSwapChain::VulkanSwapChain()
	: mActiveVkInstance{VK_NULL_HANDLE}
	, mActiveVulkanDevice{VK_NULL_HANDLE}
	, mVkSurfaceKHR{VK_NULL_HANDLE}
	, mColorVkFormat{VK_FORMAT_UNDEFINED}
	, mVkColorSpaceKHR{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
	, mVkSwapchainKHR{VK_NULL_HANDLE}
	, mVkPhysicalDevice {VK_NULL_HANDLE}
	, mQueueNodeIndex{UINT32_MAX}
	, mImageCount{0}
{
}

void VulkanSwapChain::InitializeSurface()
{
	std::uint32_t queueCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(mVkPhysicalDevice, &queueCount, nullptr);
	assert(queueCount >= 1);

	std::vector<VkQueueFamilyProperties> queueProps(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(mVkPhysicalDevice, &queueCount, queueProps.data());

	// Iterate over each queue to learn whether it supports presenting:
	// Find a queue with present support
	// Will be used to present the swap chain images to the windowing system
	std::vector<VkBool32> supportsPresent(queueCount);
	for (std::uint32_t i = 0; i < queueCount; i++)
	{
		VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(mVkPhysicalDevice, i, mVkSurfaceKHR, &supportsPresent[i]));
	}

	// Search for a graphics and a present queue in the array of queue
	// families, try to find one that supports both
	std::uint32_t graphicsQueueNodeIndex = UINT32_MAX;
	std::uint32_t presentQueueNodeIndex = UINT32_MAX;
	for (std::uint32_t i = 0; i < queueCount; i++)
	{
		if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			if (graphicsQueueNodeIndex == UINT32_MAX)
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
	if (presentQueueNodeIndex == UINT32_MAX)
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
	if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
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
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(mVkPhysicalDevice, mVkSurfaceKHR, &formatCount, NULL));
	assert(formatCount > 0);

	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(mVkPhysicalDevice, mVkSurfaceKHR, &formatCount, surfaceFormats.data()));

	// We want to get a format that best suits our needs, so we try to get one from a set of preferred formats
	// Initialize the format to the first one returned by the implementation in case we can't find one of the preffered formats
	VkSurfaceFormatKHR selectedFormat = surfaceFormats[0];
	std::vector<VkFormat> preferredImageFormats = {
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32
	};

	for (VkSurfaceFormatKHR& availableFormat : surfaceFormats)
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

void VulkanSwapChain::setContext(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
{
	this->mActiveVkInstance = instance;
	this->mVkPhysicalDevice = physicalDevice;
	this->mActiveVulkanDevice = device;
}

void VulkanSwapChain::CreateSwapchain(std::uint32_t& width, std::uint32_t& height, bool vsync)
{
	assert(mVkPhysicalDevice);
	assert(mActiveVulkanDevice);
	assert(mActiveVkInstance);

	// Store the current swap chain handle so we can use it later on to ease up recreation
	VkSwapchainKHR oldSwapchain = mVkSwapchainKHR;

	// Get physical device surface properties and formats
	VkSurfaceCapabilitiesKHR surfCaps;
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mVkPhysicalDevice, mVkSurfaceKHR, &surfCaps));

	VkExtent2D swapchainExtent = {};
	// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
	if (surfCaps.currentExtent.width == static_cast<std::uint32_t>(-1))
	{
		// If the surface size is undefined, the size is set to the size of the images requested
		swapchainExtent.width = width;
		swapchainExtent.height = height;
	}
	else
	{
		// If the surface size is defined, the swap chain size must match
		swapchainExtent = surfCaps.currentExtent;
		width = surfCaps.currentExtent.width;
		height = surfCaps.currentExtent.height;
	}


	// Select a present mode for the swapchain
	std::uint32_t presentModeCount;
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(mVkPhysicalDevice, mVkSurfaceKHR, &presentModeCount, NULL));
	assert(presentModeCount > 0);

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(mVkPhysicalDevice, mVkSurfaceKHR, &presentModeCount, presentModes.data()));

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
	if (!vsync)
	{
		for (size_t i = 0; i < presentModeCount; i++)
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
	std::uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
	if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
	{
		desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
	}

	// Find the transformation of the surface
	VkSurfaceTransformFlagsKHR preTransform;
	if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		// We prefer a non-rotated transform
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		preTransform = surfCaps.currentTransform;
	}

	// Find a supported composite alpha format (not all devices support alpha opaque)
	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	// Simply select the first composite alpha format available
	std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};
	for (VkCompositeAlphaFlagBitsKHR& compositeAlphaFlag : compositeAlphaFlags)
	{
		if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag)
		{
			compositeAlpha = compositeAlphaFlag;
			break;
		};
	}

	VkSwapchainCreateInfoKHR swapchainCI = {};
	swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCI.surface = mVkSurfaceKHR;
	swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
	swapchainCI.imageFormat = mColorVkFormat;
	swapchainCI.imageColorSpace = mVkColorSpaceKHR;
	swapchainCI.imageExtent = {swapchainExtent.width, swapchainExtent.height};
	swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
	swapchainCI.imageArrayLayers = 1;
	swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCI.queueFamilyIndexCount = 0;
	swapchainCI.presentMode = swapchainPresentMode;
	// Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
	swapchainCI.oldSwapchain = oldSwapchain;
	// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
	swapchainCI.clipped = VK_TRUE;
	swapchainCI.compositeAlpha = compositeAlpha;

	// Enable transfer source on swap chain images if supported
	if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
	{
		swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	// Enable transfer destination on swap chain images if supported
	if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	{
		swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	VK_CHECK_RESULT(vkCreateSwapchainKHR(mActiveVulkanDevice, &swapchainCI, nullptr, &mVkSwapchainKHR));

	// If an existing swap chain is re-created, destroy the old swap chain and the ressources owned by the application (image views, images are owned by the swap chain)
	if (oldSwapchain != VK_NULL_HANDLE)
	{
		for (size_t i = 0; i < mVkImages.size(); i++)
		{
			vkDestroyImageView(mActiveVulkanDevice, mVkImageViews[i], nullptr);
		}
		vkDestroySwapchainKHR(mActiveVulkanDevice, oldSwapchain, nullptr);
	}
	VK_CHECK_RESULT(vkGetSwapchainImagesKHR(mActiveVulkanDevice, mVkSwapchainKHR, &mImageCount, nullptr));

	// Get the swap chain images
	mVkImages.resize(mImageCount);
	VK_CHECK_RESULT(vkGetSwapchainImagesKHR(mActiveVulkanDevice, mVkSwapchainKHR, &mImageCount, mVkImages.data()));

	// Get the swap chain buffers containing the image and imageview
	mVkImageViews.resize(mImageCount);
	for (size_t i = 0; i < mVkImages.size(); i++)
	{
		VkImageViewCreateInfo colorAttachmentView = {};
		colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		colorAttachmentView.pNext = NULL;
		colorAttachmentView.format = mColorVkFormat;
		colorAttachmentView.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		};
		colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentView.subresourceRange.baseMipLevel = 0;
		colorAttachmentView.subresourceRange.levelCount = 1;
		colorAttachmentView.subresourceRange.baseArrayLayer = 0;
		colorAttachmentView.subresourceRange.layerCount = 1;
		colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorAttachmentView.flags = 0;
		colorAttachmentView.image = mVkImages[i];
		VK_CHECK_RESULT(vkCreateImageView(mActiveVulkanDevice, &colorAttachmentView, nullptr, &mVkImageViews[i]));
	}
}

VkResult VulkanSwapChain::acquireNextImage(VkSemaphore presentCompleteSemaphore, std::uint32_t& imageIndex) const
{
	// By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
	// With that we don't have to handle VK_NOT_READY
	return vkAcquireNextImageKHR(mActiveVulkanDevice, mVkSwapchainKHR, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, &imageIndex);
}

void VulkanSwapChain::cleanup()
{
	if (mVkSwapchainKHR != VK_NULL_HANDLE)
	{
		for (size_t i = 0; i < mVkImages.size(); i++)
			vkDestroyImageView(mActiveVulkanDevice, mVkImageViews[i], nullptr);

		vkDestroySwapchainKHR(mActiveVulkanDevice, mVkSwapchainKHR, nullptr);
	}

	if (mVkSurfaceKHR != VK_NULL_HANDLE)
		vkDestroySurfaceKHR(mActiveVkInstance, mVkSurfaceKHR, nullptr);
	
	mVkSurfaceKHR = VK_NULL_HANDLE;
	mVkSwapchainKHR = VK_NULL_HANDLE;
}
