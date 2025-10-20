#pragma once

#include "VulkanDevice.hpp"
#include "VulkanTypes.hpp"

#include <cstdint>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

class ImGuiOverlay
{
public:
	ImGuiOverlay();
	~ImGuiOverlay();

	void InitializeStyle(float aDPI);
	void PreparePipeline(const VkPipelineCache aPipelineCache, const VkFormat aColorFormat, const VkFormat aDepthFormat);
	void PrepareResources();
	void Update(std::uint32_t aCurrentBufferIndex);
	void Draw(const VkCommandBuffer aVkCommandBuffer, std::uint32_t aCurrentBufferIndex);
	void Resize(std::uint32_t aWidth, std::uint32_t aHeight);
	void FreeResources();

	void SetVulkanDevice(VulkanDevice* aVulkanDevice) { mVulkanDevice = aVulkanDevice; }
	void SetVkQueue(VkQueue aVkQueue) { mQueue = aVkQueue; }
	void SetMaxConcurrentFrames(std::uint32_t aMaxConcurrentFrames) { gMaxConcurrentFrames = aMaxConcurrentFrames; }
	void AddShader(const VkPipelineShaderStageCreateInfo& aCreateInfo) { mShaders.push_back(aCreateInfo); }

	bool WantsToCaptureInput() const;
	bool IsVisible() const { return mIsVisible;}
	float GetScale() const { return mScale; }

	void Vec2Text(const char* aLabel, const glm::vec2& aVec2);
	void Vec3Text(const char* aLabel, const glm::vec3& aVec3);
	void Vec4Text(const char* aLabel, const glm::vec4& aVec4);
	void Mat4Text(const char* aLabel, const glm::mat4& aMat4);

private:
	struct Buffers
	{
		Buffers() : vertexCount{0}, indexCount{0} {}

		VulkanBuffer vertexBuffer;
		VulkanBuffer indexBuffer;
		std::int32_t vertexCount;
		std::int32_t indexCount;
	};

	struct PushConstBlock
	{
		glm::vec2 scale{};
		glm::vec2 translate{};
	};

	PushConstBlock mPushConstBlock{};
	VkQueue mQueue;
	VkDescriptorPool mDescriptorPool;
	VkDescriptorSetLayout mDescriptorSetLayout;
	VkDescriptorSet mDescriptorSet;
	VkPipelineLayout mPipelineLayout;
	VkPipeline mPipeline;
	VkDeviceMemory mFontMemory;
	VkImage mFontImage;
	VkImageView mFontImageView;
	VkSampler mSampler;
	VkSampleCountFlagBits mRasterizationSamples;
	std::vector<VkPipelineShaderStageCreateInfo> mShaders{};
	std::vector<Buffers> mBuffers{};
	std::uint32_t mSubpass;
	std::uint32_t gMaxConcurrentFrames;
	std::uint32_t mCurrentBufferIndex;
	VulkanDevice* mVulkanDevice;
	bool mIsVisible;
	float mScale;
};
