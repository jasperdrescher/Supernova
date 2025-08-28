#pragma once

#include "vulkan/vulkan_core.h"

#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

constexpr long long gDefaultFenceTimeoutNS = 100000000000;

// Macro to check and display Vulkan return results
#define VK_CHECK_RESULT(aFunction)	\
{	\
	const VkResult vkResult = (aFunction); \
	if (vkResult != VK_SUCCESS) \
	{ \
		throw std::runtime_error(std::format("Fatal error: VkResult {} in {} at line {}", VulkanTools::GetErrorString(vkResult), __FILE__, __LINE__)); \
	} \
}

const std::string GetShaderBasePath();

namespace VulkanTools
{
	/** @brief Setting this path chnanges the place where the samples looks for assets and shaders */
	extern std::string gResourcePath;

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

	// Display error message and exit on fatal error
	void ExitFatal(const std::string& aMessage, std::int32_t aExitCode);
	void ExitFatal(const std::string& aMessage, VkResult aResultCode);

	// Load a SPIR-V shader (binary)
	VkShaderModule LoadShader(const char* aFilename, VkDevice aVkDevice);
}
