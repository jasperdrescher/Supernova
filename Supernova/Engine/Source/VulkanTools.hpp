#pragma once

#include "vulkan/vulkan_core.h"

#include "VulkanInitializers.hpp"

#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>

constexpr long long gDefaultFenceTimeoutNS = 100000000000;
constexpr int gVkFlagsNone = 0;

// Macro to check and display Vulkan return results
#define VK_CHECK_RESULT(aFunction)	\
{	\
	const VkResult vkResult = (aFunction); \
	if (vkResult != VK_SUCCESS) \
	{ \
		throw std::runtime_error(std::format("Fatal error: VkResult {} in {} at line {}", VulkanTools::GetErrorString(vkResult), __FILE__, __LINE__)); \
	} \
}

namespace VulkanTools
{
	enum class ShaderType
	{
		GLSL,
		Slang
	};

	static ShaderType gShaderType{ShaderType::GLSL};
	static std::filesystem::path gResourcesPath = "Resources/";
	static std::filesystem::path gShadersPath = gResourcesPath / "Shaders/GLSL/";

	/** @brief Returns an error code as a string */
	std::string GetErrorString(VkResult aErrorCode);

	// Selected a suitable supported depth format starting with 32 bit down to 16 bit
	// Returns false if none of the depth formats in the list is supported by the device
	VkBool32 GetSupportedDepthFormat(VkPhysicalDevice aVkPhysicalDevice, VkFormat* aVkDepthFormat);

	// Same as getSupportedDepthFormat but will only select formats that also have stencil
	VkBool32 GetSupportedDepthStencilFormat(VkPhysicalDevice aVkPhysicalDevice, VkFormat* aVkStencilFormat);

	/** @brief Insert an image memory barrier into the command buffer */
	void InsertImageMemoryBarrier(
		VkCommandBuffer aVkCommandBuffer,
		VkImage aVkImage,
		VkAccessFlags aSourceVkAccessMask,
		VkAccessFlags aDestinationVkAccessMask,
		VkImageLayout aOldVkImageLayout,
		VkImageLayout aNewVkImageLayout,
		VkPipelineStageFlags aSourceVkPipelineStageMask,
		VkPipelineStageFlags aDestinationVkPipelineStageMask,
		VkImageSubresourceRange aVkImageSubresourceRange);

	// Load a SPIR-V shader (binary)
	VkShaderModule LoadShader(const std::filesystem::path& aPath, VkDevice aVkDevice);

	// Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
	void setImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkImageSubresourceRange subresourceRange,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	// Uses a fixed sub resource layout with first mip level and layer
	void setImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}
