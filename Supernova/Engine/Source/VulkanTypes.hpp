#pragma once

#include <glm/fwd.hpp>
#include <vulkan/vulkan_core.h>

#include <glm/mat4x4.hpp>
#include <string>
#include <cstdint>
#include <string>

struct VulkanApplicationProperties
{
	VulkanApplicationProperties() : mAPIVersion(VK_API_VERSION_1_0), mWindowWidth(1280), mWindowHeight(720), mIsMinimized(false), mIsFocused(false), mIsVSyncEnabled(false), mIsValidationEnabled(false) {}

	std::string mWindowTitle;
	std::string mApplicationName;
	std::string mEngineName;
	std::uint32_t mAPIVersion;
	int mWindowWidth;
	int mWindowHeight;
	bool mIsMinimized;
	bool mIsFocused;
	bool mIsVSyncEnabled;
	bool mIsValidationEnabled;
};

struct VulkanVertex
{
	float mVertexPosition[3];
	float mVertexColor[3];
};

struct VulkanBuffer
{
	VkDevice device;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkDescriptorBufferInfo descriptor;
	VkDeviceSize size = 0;
	VkDeviceSize alignment = 0;
	void* mapped = nullptr;
	/** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
	VkBufferUsageFlags usageFlags;
	/** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
	VkMemoryPropertyFlags memoryPropertyFlags;
	uint64_t deviceAddress;
	VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void unmap();
	VkResult bind(VkDeviceSize offset = 0);
	void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void copyTo(void* data, VkDeviceSize size);
	VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void destroy();
};

struct VulkanShaderData
{
	glm::mat4 mProjectionMatrix;
	glm::mat4 mModelMatrix;
	glm::mat4 mViewMatrix;
};

struct VulkanDepthStencil
{
	VulkanDepthStencil() : mVkImage{VK_NULL_HANDLE}, mVkDeviceMemory{VK_NULL_HANDLE}, mVkImageView{VK_NULL_HANDLE} {}

	VkImage mVkImage;
	VkDeviceMemory mVkDeviceMemory;
	VkImageView mVkImageView;
};
