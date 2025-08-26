#pragma once

#include "vulkan/vulkan_core.h"

#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

// Custom define for better code readability
#define VK_FLAGS_NONE 0
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

// Macro to check and display Vulkan return results
#define VK_CHECK_RESULT(aFunction)	\
{	\
	const VkResult vkResult = (aFunction); \
	if (vkResult != VK_SUCCESS) \
	{ \
		throw std::runtime_error(std::format("Fatal error: VkResult {} in {} at line {}", vks::tools::errorString(vkResult), __FILE__, __LINE__)); \
	} \
}

const std::string getShaderBasePath();

namespace vks
{
	namespace tools
	{
		/** @brief Setting this path chnanges the place where the samples looks for assets and shaders */
		extern std::string resourcePath;

		/** @brief Disable message boxes on fatal errors */
		extern bool errorModeSilent;

		/** @brief Returns an error code as a string */
		std::string errorString(VkResult errorCode);

		// Selected a suitable supported depth format starting with 32 bit down to 16 bit
		// Returns false if none of the depth formats in the list is supported by the device
		VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat* depthFormat);
		// Same as getSupportedDepthFormat but will only select formats that also have stencil
		VkBool32 getSupportedDepthStencilFormat(VkPhysicalDevice physicalDevice, VkFormat* depthStencilFormat);

		/** @brief Insert an image memory barrier into the command buffer */
		void insertImageMemoryBarrier(
			VkCommandBuffer cmdbuffer,
			VkImage image,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask,
			VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkImageSubresourceRange subresourceRange);

		// Display error message and exit on fatal error
		void exitFatal(const std::string& message, int32_t exitCode);
		void exitFatal(const std::string& message, VkResult resultCode);

		// Load a SPIR-V shader (binary)
		VkShaderModule loadShader(const char* fileName, VkDevice device);
	}
}
