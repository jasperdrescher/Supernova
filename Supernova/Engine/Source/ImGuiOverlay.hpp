#pragma once

#include "VulkanDevice.hpp"
#include "VulkanTypes.hpp"

#include <cstdint>
#include <glm/vec2.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vks
{
	class UIOverlay
	{
	public:
		VulkanDevice* device{nullptr};
		VkQueue queue{VK_NULL_HANDLE};

		VkSampleCountFlagBits rasterizationSamples{VK_SAMPLE_COUNT_1_BIT};
		std::uint32_t subpass{0};

		struct Buffers
		{
			VulkanBuffer vertexBuffer;
			VulkanBuffer indexBuffer;
			std::int32_t vertexCount{0};
			std::int32_t indexCount{0};
		};
		std::vector<Buffers> buffers;
		std::uint32_t maxConcurrentFrames{0};
		std::uint32_t currentBuffer{0};

		std::vector<VkPipelineShaderStageCreateInfo> shaders;

		VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
		VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
		VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
		VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
		VkPipeline pipeline{VK_NULL_HANDLE};

		VkDeviceMemory fontMemory{VK_NULL_HANDLE};
		VkImage fontImage{VK_NULL_HANDLE};
		VkImageView fontView{VK_NULL_HANDLE};
		VkSampler sampler{VK_NULL_HANDLE};

		struct PushConstBlock
		{
			glm::vec2 scale;
			glm::vec2 translate;
		} pushConstBlock;

		bool visible{true};
		float scale{1.0f};

		UIOverlay();
		~UIOverlay();

		void preparePipeline(const VkPipelineCache pipelineCache, const VkRenderPass renderPass, const VkFormat colorFormat, const VkFormat depthFormat);
		void prepareResources();

		void update(std::uint32_t currentBuffer);
		void draw(const VkCommandBuffer commandBuffer, std::uint32_t currentBuffer);
		void resize(std::uint32_t width, std::uint32_t height);

		void freeResources();

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
	};
}
