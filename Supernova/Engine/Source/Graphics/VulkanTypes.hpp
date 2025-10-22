#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <ktx.h>
#include <vulkan/vulkan_core.h>

static constexpr std::uint32_t gMaxConcurrentFrames = 2;
static constexpr int gModelInstanceCount = 64;
static constexpr int gMaxLOD = 5;

struct VulkanDevice;

struct VulkanVertex
{
	float mVertexPosition[3];
	float mVertexColor[3];
};

struct VulkanUniformData
{
	VulkanUniformData() : mProjectionMatrix{}, mViewMatrix{}, mViewPosition{0.0f}, mLightPosition{0.0f, -5.0f, 0.0f, 1.0f}, mFrustumPlanes{}, mLightIntensity{1.8f} {}

	glm::mat4 mProjectionMatrix;
	glm::mat4 mViewMatrix;
	glm::vec4 mViewPosition;
	glm::vec4 mLightPosition;
	glm::vec4 mFrustumPlanes[6];
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
	VulkanInstanceData() : mPosition{}, mScale{0.0f} {}

	glm::vec3 mPosition;
	float mScale;
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
	enum class Side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
	std::array<glm::vec4, 6> mPlanes{};

	void UpdateFrustum(const glm::mat4& aMatrix);

	bool IsInSphere(const glm::vec3& aPosition, float aRadius) const;
};

struct ComputeContext
{
	VulkanBuffer mLoDBuffers; // Contains index start and counts for the different lod levels
	VkQueue mQueue; // Separate queue for compute commands (queue family may differ from the one used for graphics)
	VkCommandPool mCommandPool; // Use a separate command pool (queue family may differ from the one used for graphics)
	std::array<VkCommandBuffer, gMaxConcurrentFrames> mCommandBuffers; // Command buffer storing the dispatch commands and barriers
	std::array<VkFence, gMaxConcurrentFrames> mFences; // Synchronization fence to avoid rewriting compute CB if still in use
	struct ComputeSemaphores
	{
		VkSemaphore mReadySemaphore{VK_NULL_HANDLE};
		VkSemaphore mCompleteSemaphore{VK_NULL_HANDLE};
	};
	std::array<ComputeSemaphores, gMaxConcurrentFrames> mSemaphores{}; // Used as a wait semaphore for graphics submission
	VkDescriptorSetLayout mDescriptorSetLayout; // Compute shader binding layout
	std::array<VkDescriptorSet, gMaxConcurrentFrames> mDescriptorSets{}; // Compute shader bindings
	VkPipelineLayout mPipelineLayout; // Layout of the compute pipeline
	VkPipeline mPipeline; // Compute pipeline
};
