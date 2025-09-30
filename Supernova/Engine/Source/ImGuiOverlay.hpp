#pragma once

#include "VulkanDevice.hpp"
#include "VulkanTypes.hpp"

#include <cstdint>
#include <glm/vec2.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

class ImGuiOverlay
{
public:
	ImGuiOverlay();
	~ImGuiOverlay();

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

	bool IsVisible() const { return mIsVisible;}
	float GetScale() const { return mScale; }

	bool header(const char* caption);
	bool checkBox(const char* caption, bool* value);
	bool checkBox(const char* caption, std::int32_t* value);
	bool radioButton(const char* caption, bool value);
	bool inputFloat(const char* caption, float* value, float step, const char* format);
	bool sliderFloat(const char* caption, float* value, float min, float max);
	bool sliderInt(const char* caption, std::int32_t* value, std::int32_t min, std::int32_t max);
	bool comboBox(const char* caption, std::int32_t* itemindex, std::vector<std::string> items);
	bool button(const char* caption);
	bool colorPicker(const char* caption, float* color);
	void text(const char* formatstr, ...);

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
