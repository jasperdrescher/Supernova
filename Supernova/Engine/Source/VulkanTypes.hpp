#pragma once

#include <glm/fwd.hpp>
#include <vulkan/vulkan_core.h>

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
	VulkanBuffer() : mVkBuffer{VK_NULL_HANDLE}, mVkDeviceMemory{VK_NULL_HANDLE} {}

	VkBuffer mVkBuffer;
	VkDeviceMemory mVkDeviceMemory;
};

struct VulkanUniformBuffer : VulkanBuffer
{
	VulkanUniformBuffer() : VulkanBuffer(), mVkDescriptorSet{VK_NULL_HANDLE}, mMappedData(nullptr) {}

	// The descriptor set stores the resources bound to the binding points in a shader
	// It connects the binding points of the different shaders with the buffers and images used for those bindings
	VkDescriptorSet mVkDescriptorSet;
	std::uint8_t* mMappedData; // We keep a pointer to the mapped buffer, so we can easily update it's contents via a memcpy
};

struct VulkanShaderData
{
	glm::mat4 projectionMatrix;
	glm::mat4 modelMatrix;
	glm::mat4 viewMatrix;
};

struct VulkanDepthStencil
{
	VulkanDepthStencil() : mVkImage{VK_NULL_HANDLE}, mVkDeviceMemory{VK_NULL_HANDLE}, mVkImageView{VK_NULL_HANDLE} {}

	VkImage mVkImage;
	VkDeviceMemory mVkDeviceMemory;
	VkImageView mVkImageView;
};
