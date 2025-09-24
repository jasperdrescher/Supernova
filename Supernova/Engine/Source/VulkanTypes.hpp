#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <string>

struct VulkanVertex
{
	float mVertexPosition[3];
	float mVertexColor[3];
};

struct VulkanUniformData
{
	glm::mat4 mProjectionMatrix;
	glm::mat4 mModelViewMatrix;
	glm::vec4 mViewPosition;
};

struct VulkanBuffer
{
	VulkanBuffer() : mLogicalVkDevice{VK_NULL_HANDLE}, mVkBuffer{VK_NULL_HANDLE}, mVkDeviceMemory{VK_NULL_HANDLE}, mVkDeviceSize{0}, mVkDeviceAlignment{0}, mMappedData{nullptr}, mDeviceAddress{0} {}

	VkResult Map(VkDeviceSize aSize = VK_WHOLE_SIZE, VkDeviceSize aOffset = 0);
	void Unmap();
	VkResult Bind(VkDeviceSize aOffset = 0);
	void SetupDescriptor(VkDeviceSize aSize = VK_WHOLE_SIZE, VkDeviceSize aOffset = 0);
	void CopyTo(void* aData, VkDeviceSize aSize) const;
	VkResult Flush(VkDeviceSize aSize = VK_WHOLE_SIZE, VkDeviceSize aOffset = 0) const;
	VkResult Invalidate(VkDeviceSize aSize = VK_WHOLE_SIZE, VkDeviceSize aOffset = 0) const;
	void Destroy();

	VkDevice mLogicalVkDevice;
	VkBuffer mVkBuffer;
	VkDeviceMemory mVkDeviceMemory;
	VkDescriptorBufferInfo mVkDescriptorBufferInfo{};
	VkDeviceSize mVkDeviceSize;
	VkDeviceSize mVkDeviceAlignment;
	void* mMappedData;
	VkBufferUsageFlags mUsageFlags{}; // Usage flags to be filled by external source at buffer creation (to query at some later point)
	VkMemoryPropertyFlags mMemoryPropertyFlags{}; // Memory property flags to be filled by external source at buffer creation (to query at some later point)
	std::uint64_t mDeviceAddress;
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
