#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <vector>

struct VulkanDevice;

class VulkanSwapChain
{
public:
	VulkanSwapChain();

	void InitializeSurface();

	void SetContext(VkInstance aVkInstance, VulkanDevice* aVulkanDevice);

	/**
	* Create the swapchain and get its images with given width and height
	*
	* @param width Pointer to the width of the swapchain (may be adjusted to fit the requirements of the swapchain)
	* @param height Pointer to the height of the swapchain (may be adjusted to fit the requirements of the swapchain)
	* @param vsync (Optional, default = false) Can be used to force vsync-ed rendering (by using VK_PRESENT_MODE_FIFO_KHR as presentation mode)
	*/
	void CreateSwapchain(std::uint32_t& width, std::uint32_t& height, bool vsync = false);

	/**
	* Acquires the next image in the swap chain
	*
	* @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is ready for use
	* @param imageIndex Pointer to the image index that will be increased if the next image could be acquired
	*
	* @note The function will always wait until the next image has been acquired by setting timeout to UINT64_MAX
	*
	* @return VkResult of the image acquisition
	*/
	VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, std::uint32_t& imageIndex) const;
	
	void CleanUp();

	VkFormat mColorVkFormat;
	VkColorSpaceKHR mVkColorSpaceKHR;
	VkSwapchainKHR mVkSwapchainKHR;
	VkSurfaceKHR mVkSurfaceKHR;
	std::vector<VkImage> mVkImages;
	std::vector<VkImageView> mVkImageViews;
	std::uint32_t mQueueNodeIndex;
	std::uint32_t mImageCount;

private:
	VkInstance mActiveVkInstance;
	VulkanDevice* mActiveVulkanDevice;
};
