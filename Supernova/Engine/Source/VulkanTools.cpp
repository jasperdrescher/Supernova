#include "VulkanTools.hpp"

#include "VulkanInitializers.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

const std::string GetShaderBasePath()
{
	if (VulkanTools::gResourcePath != "")
	{
		return VulkanTools::gResourcePath + "/Shaders/";
	}

	return "./../Shaders/";
}

namespace VulkanTools
{
	std::string gResourcePath = "Resources";

	std::string GetErrorString(VkResult aErrorCode)
	{
		switch (aErrorCode)
		{
#define STR(r) case VK_ ##r: return #r
			STR(NOT_READY);
			STR(TIMEOUT);
			STR(EVENT_SET);
			STR(EVENT_RESET);
			STR(INCOMPLETE);
			STR(ERROR_OUT_OF_HOST_MEMORY);
			STR(ERROR_OUT_OF_DEVICE_MEMORY);
			STR(ERROR_INITIALIZATION_FAILED);
			STR(ERROR_DEVICE_LOST);
			STR(ERROR_MEMORY_MAP_FAILED);
			STR(ERROR_LAYER_NOT_PRESENT);
			STR(ERROR_EXTENSION_NOT_PRESENT);
			STR(ERROR_FEATURE_NOT_PRESENT);
			STR(ERROR_INCOMPATIBLE_DRIVER);
			STR(ERROR_TOO_MANY_OBJECTS);
			STR(ERROR_FORMAT_NOT_SUPPORTED);
			STR(ERROR_SURFACE_LOST_KHR);
			STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
			STR(SUBOPTIMAL_KHR);
			STR(ERROR_OUT_OF_DATE_KHR);
			STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
			STR(ERROR_VALIDATION_FAILED_EXT);
			STR(ERROR_INVALID_SHADER_NV);
			STR(ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);
#undef STR
			default:
				return "UNKNOWN_ERROR";
		}
	}

	VkBool32 GetSupportedDepthFormat(VkPhysicalDevice aVkPhysicalDevice, VkFormat* aVkDepthFormat)
	{
		// Since all depth formats may be optional, we need to find a suitable depth format to use
		// Start with the highest precision packed format
		std::vector<VkFormat> formatList = {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM
		};

		for (VkFormat& format : formatList)
		{
			VkFormatProperties formatProps;
			vkGetPhysicalDeviceFormatProperties(aVkPhysicalDevice, format, &formatProps);
			if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			{
				*aVkDepthFormat = format;
				return true;
			}
		}

		return false;
	}

	VkBool32 GetSupportedDepthStencilFormat(VkPhysicalDevice aVkPhysicalDevice, VkFormat* aVkStencilFormat)
	{
		std::vector<VkFormat> formatList = {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
		};

		for (VkFormat& format : formatList)
		{
			VkFormatProperties formatProps;
			vkGetPhysicalDeviceFormatProperties(aVkPhysicalDevice, format, &formatProps);
			if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			{
				*aVkStencilFormat = format;
				return true;
			}
		}

		return false;
	}

	void InsertImageMemoryBarrier(
		VkCommandBuffer aVkCommandBuffer,
		VkImage aVkImage,
		VkAccessFlags aSourceVkAccessMask,
		VkAccessFlags aDestinationVkAccessMask,
		VkImageLayout aOldVkImageLayout,
		VkImageLayout aNewVkImageLayout,
		VkPipelineStageFlags aSourceVkPipelineStageMask,
		VkPipelineStageFlags aDestinationVkPipelineStageMask,
		VkImageSubresourceRange aVkImageSubresourceRange)
	{
		VkImageMemoryBarrier imageMemoryBarrier = VulkanInitializers::ImageMemoryBarrier();
		imageMemoryBarrier.srcAccessMask = aSourceVkAccessMask;
		imageMemoryBarrier.dstAccessMask = aDestinationVkAccessMask;
		imageMemoryBarrier.oldLayout = aOldVkImageLayout;
		imageMemoryBarrier.newLayout = aNewVkImageLayout;
		imageMemoryBarrier.image = aVkImage;
		imageMemoryBarrier.subresourceRange = aVkImageSubresourceRange;

		vkCmdPipelineBarrier(
			aVkCommandBuffer,
			aSourceVkPipelineStageMask,
			aDestinationVkPipelineStageMask,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier);
	}

	void ExitFatal(const std::string& aMessage, std::int32_t aExitCode)
	{
		std::cerr << aMessage << "\n";
		exit(aExitCode);
	}

	void ExitFatal(const std::string& aMessage, VkResult aResultCode)
	{
		ExitFatal(aMessage, static_cast<std::int32_t>(aResultCode));
	}

	VkShaderModule LoadShader(const char* aFilename, VkDevice aVkDevice)
	{
		std::ifstream is(aFilename, std::ios::binary | std::ios::in | std::ios::ate);

		if (is.is_open())
		{
			size_t size = is.tellg();
			is.seekg(0, std::ios::beg);
			char* shaderCode = new char[size];
			is.read(shaderCode, size);
			is.close();

			assert(size > 0);

			VkShaderModule shaderModule;
			VkShaderModuleCreateInfo moduleCreateInfo{};
			moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			moduleCreateInfo.codeSize = size;
			moduleCreateInfo.pCode = (std::uint32_t*)shaderCode;

			VK_CHECK_RESULT(vkCreateShaderModule(aVkDevice, &moduleCreateInfo, nullptr, &shaderModule));

			delete[] shaderCode;

			return shaderModule;
		}
		else
		{
			std::cerr << "Error: Could not open shader file \"" << aFilename << "\"" << "\n";
			return VK_NULL_HANDLE;
		}
	}
}
