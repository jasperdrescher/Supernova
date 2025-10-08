#include "VulkanTypes.hpp"

#include "FileLoader.hpp"
#include "VulkanDevice.hpp"
#include "VulkanInitializers.hpp"
#include "VulkanTools.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <format>
#include <ktx.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>

/**
* Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
*
* @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
* @param offset (Optional) Byte offset from beginning
*
* @return VkResult of the buffer mapping call
*/
VkResult VulkanBuffer::Map(VkDeviceSize aSize, VkDeviceSize aOffset)
{
	return vkMapMemory(mLogicalVkDevice, mVkDeviceMemory, aOffset, aSize, 0, &mMappedData);
}

/**
* Unmap a mapped memory range
*
* @note Does not return a result as vkUnmapMemory can't fail
*/
void VulkanBuffer::Unmap()
{
	if (mMappedData)
	{
		vkUnmapMemory(mLogicalVkDevice, mVkDeviceMemory);
		mMappedData = nullptr;
	}
}

/**
* Attach the allocated memory block to the buffer
*
* @param offset (Optional) Byte offset (from the beginning) for the memory region to bind
*
* @return VkResult of the bindBufferMemory call
*/
VkResult VulkanBuffer::Bind(VkDeviceSize aOffset)
{
	return vkBindBufferMemory(mLogicalVkDevice, mVkBuffer, mVkDeviceMemory, aOffset);
}

/**
* Setup the default descriptor for this buffer
*
* @param size (Optional) Size of the memory range of the descriptor
* @param offset (Optional) Byte offset from beginning
*
*/
void VulkanBuffer::SetupDescriptor(VkDeviceSize aSize, VkDeviceSize aOffset)
{
	mVkDescriptorBufferInfo.offset = aOffset;
	mVkDescriptorBufferInfo.buffer = mVkBuffer;
	mVkDescriptorBufferInfo.range = aSize;
}

/**
* Copies the specified data to the mapped buffer
*
* @param data Pointer to the data to copy
* @param size Size of the data to copy in machine units
*
*/
void VulkanBuffer::CopyTo(void* aData, VkDeviceSize aSize) const
{
	assert(mMappedData);
	std::memcpy(mMappedData, aData, aSize);
}

/**
* Flush a memory range of the buffer to make it visible to the device
*
* @note Only required for non-coherent memory
*
* @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
* @param offset (Optional) Byte offset from beginning
*
* @return VkResult of the flush call
*/
VkResult VulkanBuffer::Flush(VkDeviceSize aSize, VkDeviceSize aOffset) const
{
	VkMappedMemoryRange mappedRange{
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = mVkDeviceMemory,
		.offset = aOffset,
		.size = aSize
	};
	return vkFlushMappedMemoryRanges(mLogicalVkDevice, 1, &mappedRange);
}

/**
* Invalidate a memory range of the buffer to make it visible to the host
*
* @note Only required for non-coherent memory
*
* @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
* @param offset (Optional) Byte offset from beginning
*
* @return VkResult of the invalidate call
*/
VkResult VulkanBuffer::Invalidate(VkDeviceSize aSize, VkDeviceSize aOffset) const
{
	VkMappedMemoryRange mappedRange{
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = mVkDeviceMemory,
		.offset = aOffset,
		.size = aSize
	};
	return vkInvalidateMappedMemoryRanges(mLogicalVkDevice, 1, &mappedRange);
}

/**
* Release all Vulkan resources held by this buffer
*/
void VulkanBuffer::Destroy()
{
	if (mVkBuffer)
	{
		vkDestroyBuffer(mLogicalVkDevice, mVkBuffer, nullptr);
		mVkBuffer = VK_NULL_HANDLE;
	}
	if (mVkDeviceMemory)
	{
		vkFreeMemory(mLogicalVkDevice, mVkDeviceMemory, nullptr);
		mVkDeviceMemory = VK_NULL_HANDLE;
	}
}

void VulkanTexture::updateDescriptor()
{
	descriptor.sampler = sampler;
	descriptor.imageView = view;
	descriptor.imageLayout = imageLayout;
}

void VulkanTexture::destroy()
{
	vkDestroyImageView(device->mLogicalVkDevice, view, nullptr);
	vkDestroyImage(device->mLogicalVkDevice, image, nullptr);
	if (sampler)
	{
		vkDestroySampler(device->mLogicalVkDevice, sampler, nullptr);
	}
	vkFreeMemory(device->mLogicalVkDevice, deviceMemory, nullptr);
}

ktxResult VulkanTexture::loadKTXFile(const std::filesystem::path& aPath, ktxTexture** target)
{
	ktxResult result = KTX_SUCCESS;
	if (!FileLoader::IsFileValid(aPath))
	{
		throw std::runtime_error(std::format("Could not load texture from {}. Make sure the assets submodule has been checked out and is up-to-date. ", aPath.generic_string()));
	}
	result = ktxTexture_CreateFromNamedFile(aPath.generic_string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);
	return result;
}

/**
* Load a 2D texture including all mip levels
*
* @param filename File to load (supports .ktx)
* @param format Vulkan format of the image data stored in the file
* @param device Vulkan device to create the texture on
* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
*
*/
void VulkanTexture2D::loadFromFile(const std::filesystem::path& aPath, VkFormat format, VulkanDevice* device, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
	ktxTexture* ktxTexture;
	ktxResult result = loadKTXFile(aPath.generic_string(), &ktxTexture);
	assert(result == KTX_SUCCESS);

	this->device = device;
	width = ktxTexture->baseWidth;
	height = ktxTexture->baseHeight;
	mipLevels = ktxTexture->numLevels;

	ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
	ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

	// Get device properties for the requested texture format
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(device->mVkPhysicalDevice, format, &formatProperties);

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = ktxTextureSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VK_CHECK_RESULT(vkCreateBuffer(device->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(device->mLogicalVkDevice, stagingBuffer, &memReqs);
	VkMemoryAllocateInfo memAllocInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memReqs.size,
		// Get memory type index for a host visible buffer
		.memoryTypeIndex = device->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(device->mLogicalVkDevice, &memAllocInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(device->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	std::uint8_t* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(device->mLogicalVkDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	std::memcpy(data, ktxTextureData, ktxTextureSize);
	vkUnmapMemory(device->mLogicalVkDevice, stagingMemory);

	// Setup buffer copy regions for each mip level
	std::vector<VkBufferImageCopy> bufferCopyRegions;

	for (std::uint32_t i = 0; i < mipLevels; i++)
	{
		ktx_size_t offset;
		KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
		assert(result == KTX_SUCCESS);
		VkBufferImageCopy bufferCopyRegion{
			.bufferOffset = offset,
			.imageSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.imageExtent = {
				.width = std::max(1u, ktxTexture->baseWidth >> i),
				.height = std::max(1u, ktxTexture->baseHeight >> i),
				.depth = 1
			}
		};
		bufferCopyRegions.push_back(bufferCopyRegion);
	}

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = {.width = width, .height = height, .depth = 1 },
		.mipLevels = mipLevels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = imageUsageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	// Ensure that the TRANSFER_DST bit is set for staging
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	VK_CHECK_RESULT(vkCreateImage(device->mLogicalVkDevice, &imageCreateInfo, nullptr, &image));
	vkGetImageMemoryRequirements(device->mLogicalVkDevice, image, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device->mLogicalVkDevice, &memAllocInfo, nullptr, &deviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(device->mLogicalVkDevice, image, deviceMemory, 0));

	VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .layerCount = 1,};

	// Image barrier for optimal image (target)
	// Optimal image will be used as destination for the copy
	VulkanTools::SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	// Copy mip levels from staging buffer
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<std::uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data()
	);

	// Change texture image layout to shader read after all mip levels have been copied
	this->imageLayout = imageLayout;
	VulkanTools::SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		imageLayout,
		subresourceRange);

	device->flushCommandBuffer(copyCmd, copyQueue);

	// Clean up staging resources
	vkDestroyBuffer(device->mLogicalVkDevice, stagingBuffer, nullptr);
	vkFreeMemory(device->mLogicalVkDevice, stagingMemory, nullptr);

	ktxTexture_Destroy(ktxTexture);

	// Create a default sampler
	VkSamplerCreateInfo samplerCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.anisotropyEnable = device->mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy,
		.maxAnisotropy = device->mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy ? device->mVkPhysicalDeviceProperties.limits.maxSamplerAnisotropy : 1.0f,
		.compareOp = VK_COMPARE_OP_NEVER,
		.minLod = 0.0f,
		.maxLod = (float)mipLevels,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
	};
	VK_CHECK_RESULT(vkCreateSampler(device->mLogicalVkDevice, &samplerCreateInfo, nullptr, &sampler));

	// Create image view
	// Textures are not directly accessed by the shaders and
	// are abstracted by image views containing additional
	// information and sub resource ranges
	VkImageViewCreateInfo viewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 1 },
	};
	VK_CHECK_RESULT(vkCreateImageView(device->mLogicalVkDevice, &viewCreateInfo, nullptr, &view));

	// Update descriptor image info member that can be used for setting up descriptor sets
	updateDescriptor();
}

/**
* Creates a 2D texture from a buffer
*
* @param buffer Buffer containing texture data to upload
* @param bufferSize Size of the buffer in machine units
* @param width Width of the texture to create
* @param height Height of the texture to create
* @param format Vulkan format of the image data stored in the file
* @param device Vulkan device to create the texture on
* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
* @param (Optional) filter Texture filtering for the sampler (defaults to VK_FILTER_LINEAR)
* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
*/
void VulkanTexture2D::fromBuffer(void* buffer, VkDeviceSize bufferSize, VkFormat format, std::uint32_t texWidth, std::uint32_t texHeight, VulkanDevice* device, VkQueue copyQueue, VkFilter filter, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
	assert(buffer);

	this->device = device;
	width = texWidth;
	height = texHeight;
	mipLevels = 1;

	VkMemoryAllocateInfo memAllocInfo = VulkanInitializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = VulkanInitializers::bufferCreateInfo();
	bufferCreateInfo.size = bufferSize;
	// This buffer is used as a transfer source for the buffer copy
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK_RESULT(vkCreateBuffer(device->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	vkGetBufferMemoryRequirements(device->mLogicalVkDevice, stagingBuffer, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	// Get memory type index for a host visible buffer
	memAllocInfo.memoryTypeIndex = device->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VK_CHECK_RESULT(vkAllocateMemory(device->mLogicalVkDevice, &memAllocInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(device->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	std::uint8_t* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(device->mLogicalVkDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	std::memcpy(data, buffer, bufferSize);
	vkUnmapMemory(device->mLogicalVkDevice, stagingMemory);

	VkBufferImageCopy bufferCopyRegion{
		.bufferOffset = 0,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageExtent = {
			.width = width,
			.height = height,
			.depth = 1,
		}
	};

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = {.width = width, .height = height, .depth = 1 },
		.mipLevels = mipLevels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = imageUsageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	// Ensure that the TRANSFER_DST bit is set for staging
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	VK_CHECK_RESULT(vkCreateImage(device->mLogicalVkDevice, &imageCreateInfo, nullptr, &image));

	vkGetImageMemoryRequirements(device->mLogicalVkDevice, image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;

	memAllocInfo.memoryTypeIndex = device->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device->mLogicalVkDevice, &memAllocInfo, nullptr, &deviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(device->mLogicalVkDevice, image, deviceMemory, 0));

	VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mipLevels, .layerCount = 1};

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	// Image barrier for optimal image (target)
	// Optimal image will be used as destination for the copy
	VulkanTools::SetImageLayout(copyCmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
	// Copy mip levels from staging buffer
	vkCmdCopyBufferToImage(copyCmd, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
	// Change texture image layout to shader read after all mip levels have been copied
	this->imageLayout = imageLayout;
	VulkanTools::SetImageLayout(copyCmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout, subresourceRange);
	device->flushCommandBuffer(copyCmd, copyQueue);

	// Clean up staging resources
	vkDestroyBuffer(device->mLogicalVkDevice, stagingBuffer, nullptr);
	vkFreeMemory(device->mLogicalVkDevice, stagingMemory, nullptr);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = filter,
		.minFilter = filter,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.maxAnisotropy = 1.0f,
		.compareOp = VK_COMPARE_OP_NEVER,
		.minLod = 0.0f,
		.maxLod = 0.0f,
	};
	VK_CHECK_RESULT(vkCreateSampler(device->mLogicalVkDevice, &samplerCreateInfo, nullptr, &sampler));

	// Create image view
	VkImageViewCreateInfo viewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 },
	};
	VK_CHECK_RESULT(vkCreateImageView(device->mLogicalVkDevice, &viewCreateInfo, nullptr, &view));

	// Update descriptor image info member that can be used for setting up descriptor sets
	updateDescriptor();
}
