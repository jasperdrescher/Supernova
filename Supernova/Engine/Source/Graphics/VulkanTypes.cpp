#include "VulkanTypes.hpp"

#include "FileLoader.hpp"
#include "VulkanDevice.hpp"
#include "VulkanInitializers.hpp"
#include "VulkanTools.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <ktx.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>

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

VulkanTexture::VulkanTexture()
	: mDevice{nullptr}
	, mImage{VK_NULL_HANDLE}
	, mImageLayout{}
	, mDeviceMemory{VK_NULL_HANDLE}
	, mView{VK_NULL_HANDLE}
	, mWidth{0}
	, mHeight{0}
	, mMipLevels{0}
	, mLayerCount{0}
	, mDescriptor{VK_NULL_HANDLE}
	, mSampler{VK_NULL_HANDLE}
{
}

void VulkanTexture::UpdateDescriptor()
{
	mDescriptor.sampler = mSampler;
	mDescriptor.imageView = mView;
	mDescriptor.imageLayout = mImageLayout;
}

void VulkanTexture::Destroy()
{
	vkDestroyImageView(mDevice->mLogicalVkDevice, mView, nullptr);
	vkDestroyImage(mDevice->mLogicalVkDevice, mImage, nullptr);
	if (mSampler)
	{
		vkDestroySampler(mDevice->mLogicalVkDevice, mSampler, nullptr);
	}
	vkFreeMemory(mDevice->mLogicalVkDevice, mDeviceMemory, nullptr);
}

ktxResult VulkanTexture::LoadKTXFile(const std::filesystem::path& aPath, ktxTexture** aTargetTexture)
{
	ktxResult result = KTX_SUCCESS;
	if (!FileLoader::IsFileValid(aPath))
	{
		throw std::runtime_error(std::format("Could not load texture from {}. ", aPath.generic_string()));
	}
	result = ktxTexture_CreateFromNamedFile(aPath.generic_string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, aTargetTexture);
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
void VulkanTexture2D::LoadFromFile(const std::filesystem::path& aPath, VkFormat aFormat, VulkanDevice* aDevice, VkQueue aCopyQueue, VkImageUsageFlags aImageUsageFlags, VkImageLayout aImageLayout)
{
	ktxTexture* ktxTexture;
	const ktxResult loadResult = LoadKTXFile(aPath, &ktxTexture);
	assert(loadResult == KTX_SUCCESS);

	mDevice = aDevice;
	mWidth = ktxTexture->baseWidth;
	mHeight = ktxTexture->baseHeight;
	mMipLevels = ktxTexture->numLevels;

	ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
	ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

	// Get device properties for the requested texture format
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(mDevice->mVkPhysicalDevice, aFormat, &formatProperties);

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = mDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = ktxTextureSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VK_CHECK_RESULT(vkCreateBuffer(mDevice->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(mDevice->mLogicalVkDevice, stagingBuffer, &memReqs);
	VkMemoryAllocateInfo memAllocInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memReqs.size,
		// Get memory type index for a host visible buffer
		.memoryTypeIndex = mDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mDevice->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	std::uint8_t* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(mDevice->mLogicalVkDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	std::memcpy(data, ktxTextureData, ktxTextureSize);
	vkUnmapMemory(mDevice->mLogicalVkDevice, stagingMemory);

	// Setup buffer copy regions for each mip level
	std::vector<VkBufferImageCopy> bufferCopyRegions;

	for (std::uint32_t i = 0; i < mMipLevels; i++)
	{
		ktx_size_t offset;
		const KTX_error_code getImageOffsetResult = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
		assert(getImageOffsetResult == KTX_SUCCESS);
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
		.format = aFormat,
		.extent = {.width = mWidth, .height = mHeight, .depth = 1 },
		.mipLevels = mMipLevels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = aImageUsageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	// Ensure that the TRANSFER_DST bit is set for staging
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	VK_CHECK_RESULT(vkCreateImage(mDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &mImage));
	vkGetImageMemoryRequirements(mDevice->mLogicalVkDevice, mImage, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = mDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &mDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mDevice->mLogicalVkDevice, mImage, mDeviceMemory, 0));

	VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mMipLevels, .layerCount = 1,};

	// Image barrier for optimal image (target)
	// Optimal image will be used as destination for the copy
	VulkanTools::SetImageLayout(
		copyCmd,
		mImage,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	// Copy mip levels from staging buffer
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		mImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<std::uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data()
	);

	// Change texture image layout to shader read after all mip levels have been copied
	mImageLayout = aImageLayout;
	VulkanTools::SetImageLayout(
		copyCmd,
		mImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		aImageLayout,
		subresourceRange);

	mDevice->FlushCommandBuffer(copyCmd, aCopyQueue);

	// Clean up staging resources
	vkDestroyBuffer(mDevice->mLogicalVkDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice->mLogicalVkDevice, stagingMemory, nullptr);

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
		.anisotropyEnable = mDevice->mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy,
		.maxAnisotropy = mDevice->mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy ? mDevice->mVkPhysicalDeviceProperties.limits.maxSamplerAnisotropy : 1.0f,
		.compareOp = VK_COMPARE_OP_NEVER,
		.minLod = 0.0f,
		.maxLod = (float)mMipLevels,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
	};
	VK_CHECK_RESULT(vkCreateSampler(mDevice->mLogicalVkDevice, &samplerCreateInfo, nullptr, &mSampler));

	// Create image view
	// Textures are not directly accessed by the shaders and
	// are abstracted by image views containing additional
	// information and sub resource ranges
	VkImageViewCreateInfo viewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = mImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = aFormat,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mMipLevels, .baseArrayLayer = 0, .layerCount = 1 },
	};
	VK_CHECK_RESULT(vkCreateImageView(mDevice->mLogicalVkDevice, &viewCreateInfo, nullptr, &mView));

	// Update descriptor image info member that can be used for setting up descriptor sets
	UpdateDescriptor();

	std::cout << "Loaded texture " << aPath.filename() << std::endl;
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
void VulkanTexture2D::FromBuffer(void* aBuffer, VkDeviceSize aBufferSize, VkFormat aFormat, std::uint32_t aWidth, std::uint32_t aHeight, VulkanDevice* aDevice, VkQueue aCopyQueue, VkFilter aFilter, VkImageUsageFlags aImageUsageFlags, VkImageLayout aImageLayout)
{
	assert(aBuffer);

	mDevice = aDevice;
	mWidth = aWidth;
	mHeight = aHeight;
	mMipLevels = 1;

	VkMemoryAllocateInfo memAllocInfo = VulkanInitializers::memoryAllocateInfo();
	VkMemoryRequirements memReqs;

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = VulkanInitializers::bufferCreateInfo();
	bufferCreateInfo.size = aBufferSize;
	// This buffer is used as a transfer source for the buffer copy
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK_RESULT(vkCreateBuffer(mDevice->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	vkGetBufferMemoryRequirements(mDevice->mLogicalVkDevice, stagingBuffer, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	// Get memory type index for a host visible buffer
	memAllocInfo.memoryTypeIndex = mDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VK_CHECK_RESULT(vkAllocateMemory(mDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mDevice->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	std::uint8_t* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(mDevice->mLogicalVkDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	std::memcpy(data, aBuffer, aBufferSize);
	vkUnmapMemory(mDevice->mLogicalVkDevice, stagingMemory);

	VkBufferImageCopy bufferCopyRegion{
		.bufferOffset = 0,
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageExtent = {
			.width = mWidth,
			.height = mHeight,
			.depth = 1,
		}
	};

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = aFormat,
		.extent = {.width = mWidth, .height = mHeight, .depth = 1 },
		.mipLevels = mMipLevels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = aImageUsageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	// Ensure that the TRANSFER_DST bit is set for staging
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	VK_CHECK_RESULT(vkCreateImage(mDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &mImage));

	vkGetImageMemoryRequirements(mDevice->mLogicalVkDevice, mImage, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;

	memAllocInfo.memoryTypeIndex = mDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &mDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mDevice->mLogicalVkDevice, mImage, mDeviceMemory, 0));

	VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mMipLevels, .layerCount = 1};

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = mDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	// Image barrier for optimal image (target)
	// Optimal image will be used as destination for the copy
	VulkanTools::SetImageLayout(copyCmd, mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
	// Copy mip levels from staging buffer
	vkCmdCopyBufferToImage(copyCmd, stagingBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
	// Change texture image layout to shader read after all mip levels have been copied
	mImageLayout = aImageLayout;
	VulkanTools::SetImageLayout(copyCmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aImageLayout, subresourceRange);
	mDevice->FlushCommandBuffer(copyCmd, aCopyQueue);

	// Clean up staging resources
	vkDestroyBuffer(mDevice->mLogicalVkDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice->mLogicalVkDevice, stagingMemory, nullptr);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = aFilter,
		.minFilter = aFilter,
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
	VK_CHECK_RESULT(vkCreateSampler(mDevice->mLogicalVkDevice, &samplerCreateInfo, nullptr, &mSampler));

	// Create image view
	VkImageViewCreateInfo viewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = mImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = aFormat,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 },
	};
	VK_CHECK_RESULT(vkCreateImageView(mDevice->mLogicalVkDevice, &viewCreateInfo, nullptr, &mView));

	// Update descriptor image info member that can be used for setting up descriptor sets
	UpdateDescriptor();
}

/**
	* Load a 2D texture array including all mip levels
	*
	* @param filename File to load (supports .ktx)
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	*
	*/
void VulkanTexture2DArray::LoadFromFile(const std::filesystem::path& aPath, VkFormat aFormat, VulkanDevice* aDevice, VkQueue aCopyQueue, VkImageUsageFlags aImageUsageFlags, VkImageLayout aImageLayout)
{
	ktxTexture* ktxTexture;
	const ktxResult loadFileResult = LoadKTXFile(aPath, &ktxTexture);
	assert(loadFileResult == KTX_SUCCESS);

	mDevice = aDevice;
	mWidth = ktxTexture->baseWidth;
	mHeight = ktxTexture->baseHeight;
	mLayerCount = ktxTexture->numLayers;
	mMipLevels = ktxTexture->numLevels;

	ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
	ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = VulkanInitializers::bufferCreateInfo();
	bufferCreateInfo.size = ktxTextureSize;
	// This buffer is used as a transfer source for the buffer copy
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK_RESULT(vkCreateBuffer(mDevice->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(mDevice->mLogicalVkDevice, stagingBuffer, &memReqs);
	VkMemoryAllocateInfo memAllocInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memReqs.size,
		.memoryTypeIndex = mDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mDevice->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	uint8_t* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(mDevice->mLogicalVkDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	memcpy(data, ktxTextureData, ktxTextureSize);
	vkUnmapMemory(mDevice->mLogicalVkDevice, stagingMemory);

	// Setup buffer copy regions for each layer including all of its miplevels
	std::vector<VkBufferImageCopy> bufferCopyRegions;

	for (std::uint32_t layer = 0; layer < mLayerCount; layer++)
	{
		for (std::uint32_t level = 0; level < mMipLevels; level++)
		{
			ktx_size_t offset;
			const KTX_error_code getImageOffsetResult = ktxTexture_GetImageOffset(ktxTexture, level, layer, 0, &offset);
			assert(getImageOffsetResult == KTX_SUCCESS);
			VkBufferImageCopy bufferCopyRegion{
				.bufferOffset = offset,
				.imageSubresource {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = level,
					.baseArrayLayer = layer,
					.layerCount = 1,
				},
				.imageExtent {
					.width = ktxTexture->baseWidth >> level,
					.height = ktxTexture->baseHeight >> level,
					.depth = 1,
				}
			};
			bufferCopyRegions.push_back(bufferCopyRegion);
		}
	}

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = aFormat,
		.extent = {.width = mWidth, .height = mHeight, .depth = 1 },
		.mipLevels = mMipLevels,
		.arrayLayers = mLayerCount,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = aImageUsageFlags,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	// Ensure that the TRANSFER_DST bit is set for staging
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	VK_CHECK_RESULT(vkCreateImage(mDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &mImage));

	vkGetImageMemoryRequirements(mDevice->mLogicalVkDevice, mImage, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = mDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &mDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mDevice->mLogicalVkDevice, mImage, mDeviceMemory, 0));

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = mDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	// Image barrier for optimal image (target)
	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mMipLevels, .layerCount = mLayerCount};
	VulkanTools::SetImageLayout(copyCmd, mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
	// Copy the layers and mip levels from the staging buffer to the optimal tiled image
	vkCmdCopyBufferToImage(copyCmd, stagingBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());
	// Change texture image layout to shader read after all faces have been copied
	mImageLayout = aImageLayout;
	VulkanTools::SetImageLayout(copyCmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aImageLayout, subresourceRange);
	mDevice->FlushCommandBuffer(copyCmd, aCopyQueue);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = samplerCreateInfo.addressModeU,
		.addressModeW = samplerCreateInfo.addressModeU,
		.mipLodBias = 0.0f,
		.anisotropyEnable = mDevice->mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy,
		.maxAnisotropy = mDevice->mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy ? mDevice->mVkPhysicalDeviceProperties.limits.maxSamplerAnisotropy : 1.0f,
		.compareOp = VK_COMPARE_OP_NEVER,
		.minLod = 0.0f,
		.maxLod = (float)mMipLevels,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	};
	VK_CHECK_RESULT(vkCreateSampler(mDevice->mLogicalVkDevice, &samplerCreateInfo, nullptr, &mSampler));

	// Create image view
	VkImageViewCreateInfo viewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = mImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		.format = aFormat,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mMipLevels, .baseArrayLayer = 0, .layerCount = mLayerCount },
	};
	VK_CHECK_RESULT(vkCreateImageView(mDevice->mLogicalVkDevice, &viewCreateInfo, nullptr, &mView));

	// Clean up staging resources
	ktxTexture_Destroy(ktxTexture);
	vkDestroyBuffer(mDevice->mLogicalVkDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice->mLogicalVkDevice, stagingMemory, nullptr);

	// Update descriptor image info member that can be used for setting up descriptor sets
	UpdateDescriptor();

	std::cout << "Loaded texture " << aPath.filename() << std::endl;
}
