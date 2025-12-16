#pragma once

#if VULKAN_C

#include "Core/Types.hpp"

#include <vector>
#include <vulkan/vulkan_core.h>

struct VulkanCDevice;

class VulkanCSwapChain
{
public:
	VulkanCSwapChain();

	void InitializeSurface();
	void SetContext(VkInstance aVkInstance, VulkanCDevice* aVulkanDevice);
	void CreateSwapchain(Core::uint32& aWidth, Core::uint32& aHeight, bool aUseVSync = false);
	void CleanUp();

	VkResult AcquireNextImage(VkSemaphore aPresentCompleteSemaphore, Core::uint32& aImageIndex) const;

	VkFormat mColorVkFormat;
	VkColorSpaceKHR mVkColorSpaceKHR;
	VkSwapchainKHR mVkSwapchainKHR;
	VkSurfaceKHR mVkSurfaceKHR;
	std::vector<VkImage> mVkImages{};
	std::vector<VkImageView> mVkImageViews{};
	Core::uint32 mQueueNodeIndex;
	Core::uint32 mImageCount;

private:
	VkInstance mActiveVkInstance;
	VulkanCDevice* mActiveVulkanDevice;
};
#endif
