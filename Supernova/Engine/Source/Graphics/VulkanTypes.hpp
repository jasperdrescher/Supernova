#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
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
	VulkanUniformData() : mProjectionMatrix{}, mViewMatrix{}, mViewPosition{0.0f}, mLightPosition{0.0f, -5.0f, 0.0f, 1.0f}, mLocalSpeed{0.0f}, mGlobalSpeed{0.0f}, mLightIntensity{1.8f}, frustumPlanes{} {}

	glm::mat4 mProjectionMatrix;
	glm::mat4 mViewMatrix;
	glm::vec4 mViewPosition;
	glm::vec4 mLightPosition;
	float mLocalSpeed;
	float mGlobalSpeed;
	float mLightIntensity;
	glm::vec4 frustumPlanes[6];
	glm::vec4 cameraPos{0.0f};
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

class VulkanFrustum
{
public:
	enum side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
	std::array<glm::vec4, 6> planes;

	void update(glm::mat4 matrix)
	{
		planes[LEFT].x = matrix[0].w + matrix[0].x;
		planes[LEFT].y = matrix[1].w + matrix[1].x;
		planes[LEFT].z = matrix[2].w + matrix[2].x;
		planes[LEFT].w = matrix[3].w + matrix[3].x;

		planes[RIGHT].x = matrix[0].w - matrix[0].x;
		planes[RIGHT].y = matrix[1].w - matrix[1].x;
		planes[RIGHT].z = matrix[2].w - matrix[2].x;
		planes[RIGHT].w = matrix[3].w - matrix[3].x;

		planes[TOP].x = matrix[0].w - matrix[0].y;
		planes[TOP].y = matrix[1].w - matrix[1].y;
		planes[TOP].z = matrix[2].w - matrix[2].y;
		planes[TOP].w = matrix[3].w - matrix[3].y;

		planes[BOTTOM].x = matrix[0].w + matrix[0].y;
		planes[BOTTOM].y = matrix[1].w + matrix[1].y;
		planes[BOTTOM].z = matrix[2].w + matrix[2].y;
		planes[BOTTOM].w = matrix[3].w + matrix[3].y;

		planes[BACK].x = matrix[0].w + matrix[0].z;
		planes[BACK].y = matrix[1].w + matrix[1].z;
		planes[BACK].z = matrix[2].w + matrix[2].z;
		planes[BACK].w = matrix[3].w + matrix[3].z;

		planes[FRONT].x = matrix[0].w - matrix[0].z;
		planes[FRONT].y = matrix[1].w - matrix[1].z;
		planes[FRONT].z = matrix[2].w - matrix[2].z;
		planes[FRONT].w = matrix[3].w - matrix[3].z;

		for (auto i = 0; i < planes.size(); i++)
		{
			float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
			planes[i] /= length;
		}
	}

	bool checkSphere(glm::vec3 pos, float radius)
	{
		for (auto i = 0; i < planes.size(); i++)
		{
			if ((planes[i].x * pos.x) + (planes[i].y * pos.y) + (planes[i].z * pos.z) + planes[i].w <= -radius)
			{
				return false;
			}
		}
		return true;
	}
};
