#pragma once

#include "Core/Types.hpp"
#include "Math/Types.hpp"

#include <array>
#include <filesystem>
#include <ktx.h>
#include <vector>
#include <vulkan/vulkan_core.h>

static constexpr Core::uint32 gMaxConcurrentFrames = 2;
static constexpr int gModelInstanceCount = 64;
static constexpr int gMaxLOD = 5;

struct VulkanDevice;

struct UniformBufferData
{
	UniformBufferData() : mProjectionMatrix{}, mViewMatrix{}, mViewPosition{0.0f}, mLightPosition{0.0f}, mFrustumPlanes{}, mLightIntensity{1.8f} {}

	Math::Matrix4f mProjectionMatrix;
	Math::Matrix4f mViewMatrix;
	Math::Vector4f mViewPosition;
	Math::Vector4f mLightPosition;
	Math::Vector4f mFrustumPlanes[6];
	float mLightIntensity;
};

struct InstanceData
{
	InstanceData() : mPosition{}, mScale{0.0f} {}

	Math::Vector3f mPosition;
	float mScale;
};

struct PushConstant
{
	PushConstant() : mModelMatrix{} {}

	Math::Matrix4f mModelMatrix;
};

struct Buffer
{
	Buffer() : mLogicalVkDevice{VK_NULL_HANDLE}, mVkBuffer{VK_NULL_HANDLE}, mVkDeviceMemory{VK_NULL_HANDLE}, mVkDeviceSize{0}, mVkDeviceAlignment{0}, mMappedData{nullptr}, mDeviceAddress{0} {}

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
	Core::uint64 mDeviceAddress;
};

struct DepthStencil
{
	DepthStencil() : mVkImage{VK_NULL_HANDLE}, mVkDeviceMemory{VK_NULL_HANDLE}, mVkImageView{VK_NULL_HANDLE} {}

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
	Core::uint32 mWidth;
	Core::uint32 mHeight;
	Core::uint32 mMipLevels;
	Core::uint32 mLayerCount;
	VkDescriptorImageInfo mDescriptor;
	VkSampler mSampler;
};

class VulkanTexture2D : public VulkanTexture
{
public:
	void LoadFromFile(const std::filesystem::path& aPath, VkFormat aFormat, VulkanDevice* aDevice, VkQueue aCopyQueue, VkImageUsageFlags aImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout aImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	void FromBuffer(void* aBuffer, VkDeviceSize aBufferSize, VkFormat aFormat, Core::uint32 aWidth, Core::uint32 aHeight, VulkanDevice* aDevice, VkQueue aCopyQueue, VkFilter aFilter = VK_FILTER_LINEAR, VkImageUsageFlags aImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout aImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};

class VulkanTexture2DArray : public VulkanTexture
{
public:
	void LoadFromFile(const std::filesystem::path& aPath, VkFormat aFormat, VulkanDevice* aDevice, VkQueue aCopyQueue, VkImageUsageFlags aImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout aImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};

class ViewFrustum
{
public:
	enum class Side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
	std::array<Math::Vector4f, 6> mPlanes{};

	void UpdateFrustum(const Math::Matrix4f& aMatrix);

	bool IsInSphere(const Math::Vector3f& aPosition, float aRadius) const;
};

struct GraphicsContext
{
	GraphicsContext() : mQueue{VK_NULL_HANDLE}, mCommandPool{VK_NULL_HANDLE}, mPipelineLayout{VK_NULL_HANDLE}, mDescriptorSetLayout{VK_NULL_HANDLE} {}

	VkQueue mQueue;
	VkCommandPool mCommandPool;
	VkPipelineLayout mPipelineLayout;
	VkDescriptorSetLayout mDescriptorSetLayout;
	std::array<VkCommandBuffer, gMaxConcurrentFrames> mCommandBuffers{}; // Command buffers used for rendering
	std::array<VkFence, gMaxConcurrentFrames> mFences{};
	std::array<VkSemaphore, gMaxConcurrentFrames> mPresentCompleteSemaphores{};
	std::vector<VkSemaphore> mRenderCompleteSemaphores{};
};

struct ComputeContext
{
	struct ComputeSemaphores
	{
		ComputeSemaphores() : mReadySemaphore{VK_NULL_HANDLE}, mCompleteSemaphore{VK_NULL_HANDLE} {}

		VkSemaphore mReadySemaphore;
		VkSemaphore mCompleteSemaphore;
	};

	ComputeContext() : mQueue{VK_NULL_HANDLE}, mCommandPool{VK_NULL_HANDLE}, mDescriptorSetLayout{VK_NULL_HANDLE}, mPipelineLayout{VK_NULL_HANDLE} {}

	Buffer mLoDBuffers{}; // Contains index start and counts for the different lod levels
	VkQueue mQueue; // Separate queue for compute commands (queue family may differ from the one used for graphics)
	VkCommandPool mCommandPool; // Use a separate command pool (queue family may differ from the one used for graphics)
	std::array<VkCommandBuffer, gMaxConcurrentFrames> mCommandBuffers{}; // Command buffer storing the dispatch commands and barriers
	std::array<VkFence, gMaxConcurrentFrames> mFences{}; // Synchronization fence to avoid rewriting compute CB if still in use
	std::array<ComputeSemaphores, gMaxConcurrentFrames> mSemaphores{}; // Used as a wait semaphore for graphics submission
	VkDescriptorSetLayout mDescriptorSetLayout; // Compute shader binding layout
	std::array<VkDescriptorSet, gMaxConcurrentFrames> mDescriptorSets{}; // Compute shader bindings
	VkPipelineLayout mPipelineLayout; // Layout of the compute pipeline
	VkPipeline mPipeline{}; // Compute pipeline
};
