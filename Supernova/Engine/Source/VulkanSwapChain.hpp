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

	void setContext(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

	/**
	* Create the swapchain and get its images with given width and height
	*
	* @param width Pointer to the width of the swapchain (may be adjusted to fit the requirements of the swapchain)
	* @param height Pointer to the height of the swapchain (may be adjusted to fit the requirements of the swapchain)
	* @param vsync (Optional, default = false) Can be used to force vsync-ed rendering (by using VK_PRESENT_MODE_FIFO_KHR as presentation mode)
	*/
	void CreateSwapchain(uint32_t& width, uint32_t& height, bool vsync = false, bool fullscreen = false);
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
	VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t& imageIndex) const;
	
	/* Free all Vulkan resources acquired by the swapchain */
	void cleanup();

	VkFormat mColorVkFormat;
	VkColorSpaceKHR mVkColorSpaceKHR;
	VkSwapchainKHR mVkSwapchainKHR;
	VkSurfaceKHR mVkSurfaceKHR;
	std::vector<VkImage> mVkImages;
	std::vector<VkImageView> mVkImageViews;
	uint32_t mQueueNodeIndex;
	uint32_t mImageCount;

private:
	VkInstance mActiveVkInstance;
	VkDevice mActiveVulkanDevice;
	VkPhysicalDevice mVkPhysicalDevice;
};
