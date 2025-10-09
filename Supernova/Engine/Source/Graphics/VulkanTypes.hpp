#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <ktx.h>
#include <string>
#include <vulkan/vulkan_core.h>

struct VulkanDevice;

struct VulkanVertex
{
	float mVertexPosition[3];
	float mVertexColor[3];
};

struct VulkanUniformData
{
	VulkanUniformData() : mProjectionMatrix{}, mViewMatrix{}, mViewPosition{0.0f}, mLightPosition{0.0f, -5.0f, 0.0f, 1.0f}, mLocalSpeed{0.0f}, mGlobalSpeed{0.0f}, mLightIntensity{1.8f} {}

	glm::mat4 mProjectionMatrix;
	glm::mat4 mViewMatrix;
	glm::vec4 mViewPosition;
	glm::vec4 mLightPosition;
	float mLocalSpeed;
	float mGlobalSpeed;
	float mLightIntensity;
};

struct VulkanInstanceBuffer
{
	VulkanInstanceBuffer() : mVkBuffer{VK_NULL_HANDLE}, mVkDeviceMemory{VK_NULL_HANDLE}, mVkDescriptorBufferInfo{VK_NULL_HANDLE}, mSize{0} {}

	VkBuffer mVkBuffer;
	VkDeviceMemory mVkDeviceMemory;
	VkDescriptorBufferInfo mVkDescriptorBufferInfo;
	std::size_t mSize;
};

struct VulkanInstanceData
{
	VulkanInstanceData() : mPosition{}, mRotation{}, mScale{0.0f}, mTextureIndex{0} {}

	glm::vec3 mPosition;
	glm::vec3 mRotation;
	float mScale;
	std::uint32_t mTextureIndex;
};

struct VulkanPushConstant
{
	VulkanPushConstant() : mModelMatrix{} {}

	glm::mat4 mModelMatrix;
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
	VulkanShaderData() : mProjectionMatrix{}, mModelMatrix{}, mViewMatrix{} {}

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

class VulkanTexture
{
public:
	VulkanTexture();

	void UpdateDescriptor();
	void Destroy();

	ktxResult LoadKTXFile(const std::filesystem::path& aPath, ktxTexture** aTargetTexture);

	VulkanDevice* mDevice;
	VkImage mImage;
	VkImageLayout mImageLayout;
	VkDeviceMemory mDeviceMemory;
	VkImageView mView;
	std::uint32_t mWidth;
	std::uint32_t mHeight;
	std::uint32_t mMipLevels;
	std::uint32_t mLayerCount;
	VkDescriptorImageInfo mDescriptor;
	VkSampler mSampler;
};

class VulkanTexture2D : public VulkanTexture
{
public:
	void LoadFromFile(const std::filesystem::path& aPath, VkFormat aFormat, VulkanDevice* aDevice, VkQueue aCopyQueue, VkImageUsageFlags aImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout aImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	void FromBuffer(void* aBuffer, VkDeviceSize aBufferSize, VkFormat aFormat, std::uint32_t aWidth, std::uint32_t aHeight, VulkanDevice* aDevice, VkQueue aCopyQueue, VkFilter aFilter = VK_FILTER_LINEAR, VkImageUsageFlags aImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout aImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};

class VulkanTexture2DArray : public VulkanTexture
{
public:
	void LoadFromFile(const std::filesystem::path& aPath, VkFormat aFormat, VulkanDevice* aDevice, VkQueue aCopyQueue, VkImageUsageFlags aImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout aImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};
