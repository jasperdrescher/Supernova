#include "VulkanTools.hpp"

#include "VulkanInitializers.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace VulkanTools
{
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
			STR(ERROR_FRAGMENTED_POOL);
			STR(ERROR_UNKNOWN);
			STR(ERROR_OUT_OF_POOL_MEMORY); // VK_VERSION_1_1
			STR(ERROR_INVALID_EXTERNAL_HANDLE); // VK_VERSION_1_1
			STR(ERROR_FRAGMENTATION); // VK_VERSION_1_2
			STR(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS); // VK_VERSION_1_2
			STR(PIPELINE_COMPILE_REQUIRED); // VK_VERSION_1_3
			STR(ERROR_SURFACE_LOST_KHR); // VK_KHR_surface
			STR(ERROR_NATIVE_WINDOW_IN_USE_KHR); // VK_KHR_surface
			STR(SUBOPTIMAL_KHR); // VK_KHR_swapchain
			STR(ERROR_OUT_OF_DATE_KHR); // VK_KHR_swapchain
			STR(ERROR_INCOMPATIBLE_DISPLAY_KHR); // VK_KHR_display_swapchain
			STR(ERROR_VALIDATION_FAILED_EXT); // VK_EXT_debug_report
			STR(ERROR_INVALID_SHADER_NV); // VK_NV_glsl_shader
			STR(INCOMPATIBLE_SHADER_BINARY_EXT); // VK_EXT_shader_object
#undef STR
			default:
				return std::to_string(static_cast<int>(aErrorCode));
		}
	}

	VkBool32 GetSupportedDepthFormat(VkPhysicalDevice aVkPhysicalDevice, VkFormat* aVkDepthFormat)
	{
		// Since all depth formats may be optional, we need to find a suitable depth format to use
		// Start with the highest precision packed format
		const std::vector<VkFormat> formatList = {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM
		};

		for (const VkFormat& format : formatList)
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
		const std::vector<VkFormat> formatList = {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
		};

		for (const VkFormat& format : formatList)
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
			0,
			nullptr,
			0,
			nullptr,
			1,
			&imageMemoryBarrier);
	}

	VkShaderModule LoadShader(const std::filesystem::path& aPath, VkDevice aVkDevice)
	{
		std::ifstream inStream(aPath, std::ios::binary | std::ios::in | std::ios::ate);

		if (inStream.is_open())
		{
			const std::size_t size = inStream.tellg();
			inStream.seekg(0, std::ios::beg);
			char* shaderCode = new char[size];
			inStream.read(shaderCode, size);
			inStream.close();

			assert(size > 0);

			VkShaderModule shaderModule;
			VkShaderModuleCreateInfo shaderModuleCreateInfo{
				.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = size,
				.pCode = reinterpret_cast<std::uint32_t*>(shaderCode)
			};

			VK_CHECK_RESULT(vkCreateShaderModule(aVkDevice, &shaderModuleCreateInfo, nullptr, &shaderModule));

			delete[] shaderCode;

			std::cout << "Loaded shader " << aPath.filename() << std::endl;

			return shaderModule;
		}
		else
		{
			std::cerr << "Error: Could not open shader file \"" << aPath << "\"" << std::endl;
			return VK_NULL_HANDLE;
		}
	}

	// Create an image memory barrier for changing the layout of
	// an image and put it into an active command buffer
	void SetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkImageSubresourceRange subresourceRange,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask)
	{
		// Create an image barrier object
		VkImageMemoryBarrier imageMemoryBarrier = VulkanInitializers::ImageMemoryBarrier();
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;
		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		// Source layouts (old)
		// Source access mask controls actions that have to be finished on the old layout
		// before it will be transitioned to the new layout
		switch (oldImageLayout)
		{
			case VK_IMAGE_LAYOUT_UNDEFINED:
				// Image layout is undefined (or does not matter)
				// Only valid as initial layout
				// No flags required, listed only for completeness
				imageMemoryBarrier.srcAccessMask = 0;
				break;

			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				// Image is preinitialized
				// Only valid as initial layout for linear images, preserves memory contents
				// Make sure host writes have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image is a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image is a depth/stencil attachment
				// Make sure any writes to the depth/stencil buffer have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image is a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image is a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image is read by a shader
				// Make sure any shader reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
		}

		// Target layouts (new)
		// Destination access mask controls the dependency for the new image layout
		switch (newImageLayout)
		{
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image will be used as a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image will be used as a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image will be used as a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image layout will be used as a depth/stencil attachment
				// Make sure any writes to depth/stencil buffer have been finished
				imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image will be read in a shader (sampler, input attachment)
				// Make sure any writes to the image have been finished
				if (imageMemoryBarrier.srcAccessMask == 0)
				{
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				}
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
		}

		// Put barrier inside setup command buffer
		vkCmdPipelineBarrier(
			cmdbuffer,
			srcStageMask,
			dstStageMask,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			&imageMemoryBarrier);
	}

	// Fixed sub resource on first mip level and layer
	void SetImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask)
	{
		VkImageSubresourceRange imageSubresourceRange{
			.aspectMask = aspectMask,
			.baseMipLevel = 0,
			.levelCount = 1,
			.layerCount = 1,
		};
		
		SetImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, imageSubresourceRange, srcStageMask, dstStageMask);
	}
}
