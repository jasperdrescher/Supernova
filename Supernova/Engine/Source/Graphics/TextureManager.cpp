#include "TextureManager.hpp"

#include "Core/Types.hpp"
#include "FileLoader.cpp"
#include "Timer.hpp"
#include "VulkanDevice.hpp"
#include "VulkanGlTFTypes.hpp"
#include "VulkanTools.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <iostream>
#include <ktx.h>
#include <ktxvulkan.h>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

TextureManager::TextureManager()
	: mVulkanDevice{nullptr}
	, mTransferQueue{VK_NULL_HANDLE}
{
}

TextureManager::~TextureManager()
{
}

void TextureManager::SetContext(VulkanDevice* aDevice, VkQueue aTransferQueue)
{
	mVulkanDevice = aDevice;
	mTransferQueue = aTransferQueue;
}

vkglTF::Texture TextureManager::CreateEmptyTexture()
{
	vkglTF::Texture texture{};

	texture.mVulkanDevice = mVulkanDevice;
	texture.mWidth = 1;
	texture.mHeight = 1;
	texture.mLayerCount = 1;
	texture.mMipLevels = 1;

	const Core::size bufferSize = static_cast<Core::size>(texture.mWidth * texture.mHeight * 4);
	unsigned char* buffer = new unsigned char[bufferSize];
	std::memset(buffer, 0, bufferSize);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	const VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	};
	VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	VkMemoryRequirements hostMemoryRequirements;
	vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalVkDevice, stagingBuffer, &hostMemoryRequirements);

	const VkMemoryAllocateInfo hostMemoryAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = hostMemoryRequirements.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(hostMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &hostMemoryAllocateInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	Core::uint8* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory, 0, hostMemoryRequirements.size, 0, (void**)&data));

	std::memcpy(data, buffer, bufferSize);
	vkUnmapMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory);

	// Create optimal tiled target image
	const VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {.width = texture.mWidth, .height = texture.mHeight, .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &texture.mImage));

	VkMemoryRequirements localMemoryRequirements;
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalVkDevice, texture.mImage, &localMemoryRequirements);

	const VkMemoryAllocateInfo localMemoryAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = localMemoryRequirements.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(localMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &localMemoryAllocateInfo, nullptr, &texture.mDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalVkDevice, texture.mImage, texture.mDeviceMemory, 0));

	const VkBufferImageCopy bufferCopyRegion{
		.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
		.imageExtent = {.width = texture.mWidth, .height = texture.mHeight, .depth = 1 }
	};
	const VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .layerCount = 1};
	VkCommandBuffer copyCommandBuffer = mVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VulkanTools::SetImageLayout(copyCommandBuffer, texture.mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
	vkCmdCopyBufferToImage(copyCommandBuffer, stagingBuffer, texture.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
	VulkanTools::SetImageLayout(copyCommandBuffer, texture.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
	mVulkanDevice->FlushCommandBuffer(copyCommandBuffer, mTransferQueue, true);
	texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Clean up staging resources
	vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, stagingBuffer, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory, nullptr);

	const VkSamplerCreateInfo samplerCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.maxAnisotropy = 1.0f,
		.compareOp = VK_COMPARE_OP_NEVER,
	};
	VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->mLogicalVkDevice, &samplerCreateInfo, nullptr, &texture.mSampler));

	const VkImageViewCreateInfo viewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = texture.mImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1 },
	};
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalVkDevice, &viewCreateInfo, nullptr, &texture.mImageView));

	texture.mDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	texture.mDescriptorImageInfo.imageView = texture.mImageView;
	texture.mDescriptorImageInfo.sampler = texture.mSampler;

	return texture;
}

vkglTF::Texture TextureManager::CreateTexture(const std::filesystem::path& aPath)
{
	Time::Timer loadTimer;
	loadTimer.StartTimer();

	const bool isKtx = aPath.extension() == ".ktx";
	if (!isKtx)
	{
		throw std::runtime_error(std::format("Texture is not ktx: {}", aPath.generic_string()));
	}

	VkFormat format = VK_FORMAT_UNDEFINED;

	vkglTF::Texture texture{};
	texture.mVulkanDevice = mVulkanDevice;

	CreateFromKtxTexture(aPath, texture, format);

	CreateResources(texture, format);

	loadTimer.EndTimer();

	std::cout << "Loaded texture " << aPath.filename() << " " << std::format("({:.2f}ms)", loadTimer.GetDurationMilliseconds()) << std::endl;

	return texture;
}

vkglTF::Texture TextureManager::CreateTexture(const std::filesystem::path& aPath, vkglTF::Image& aImage)
{
	Time::Timer loadTimer;
	loadTimer.StartTimer();

	const bool isKtx = aPath.extension() == ".ktx";

	VkFormat format = VK_FORMAT_UNDEFINED;

	vkglTF::Texture texture{};
	texture.mVulkanDevice = mVulkanDevice;

	if (isKtx)
	{
		CreateFromKtxTexture(aPath / aImage.uri, texture, format);
	}
	else
	{
		CreateFromEmbeddedTexture(aImage, texture, format);
	}

	CreateResources(texture, format);

	loadTimer.EndTimer();

	std::cout << "Loaded texture " << aImage.name << " " << std::format("({:.2f}ms)", loadTimer.GetDurationMilliseconds()) << std::endl;

	return texture;
}

void TextureManager::CreateFromKtxTexture(const std::filesystem::path& aPath, vkglTF::Texture& aTexture, VkFormat& aFormat)
{
	ktxTexture* ktxTexture;

	if (!FileLoader::IsFileValid(aPath))
	{
		throw std::runtime_error(std::format("Could not load texture from: {}", aPath.generic_string()));
	}

	const ktxResult createFromNamedFileResult = ktxTexture_CreateFromNamedFile(aPath.generic_string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
	if (createFromNamedFileResult != KTX_SUCCESS)
	{
		throw std::runtime_error(std::format("Could not create named file: {}", aPath.generic_string()));
	}

	aTexture.mWidth = ktxTexture->baseWidth;
	aTexture.mHeight = ktxTexture->baseHeight;
	aTexture.mMipLevels = ktxTexture->numLevels;
	aTexture.mLayerCount = ktxTexture->numLayers;

	if (aTexture.mLayerCount > 1)
	{
		aTexture.mTextureType = vkglTF::TextureType::Array;
	}

	const ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
	const ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);
	aFormat = ktxTexture_GetVkFormat(ktxTexture);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	const VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = ktxTextureSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	VkMemoryRequirements hostMemoryRequirements;
	vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalVkDevice, stagingBuffer, &hostMemoryRequirements);

	const VkMemoryAllocateInfo hostMemoryAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = hostMemoryRequirements.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(hostMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &hostMemoryAllocateInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

	Core::uint8* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory, 0, hostMemoryRequirements.size, 0, reinterpret_cast<void**>(&data)));
	std::memcpy(data, ktxTextureData, ktxTextureSize);
	vkUnmapMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory);

	std::vector<VkBufferImageCopy> bufferCopyRegions;
	for (Core::uint32 layer = 0; layer < aTexture.mLayerCount; layer++)
	{
		for (Core::uint32 mipLevel = 0; mipLevel < aTexture.mMipLevels; mipLevel++)
		{
			ktx_size_t offset;
			const KTX_error_code getImageOffsetResult = ktxTexture_GetImageOffset(ktxTexture, mipLevel, layer, 0, &offset);
			if (getImageOffsetResult != KTX_SUCCESS)
			{
				throw std::runtime_error("Could not get image offset");
			}

			const VkBufferImageCopy bufferCopyRegion{
				.bufferOffset = offset,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = mipLevel,
					.baseArrayLayer = layer,
					.layerCount = 1
				},
				.imageExtent = {
					.width = std::max(1u, ktxTexture->baseWidth >> mipLevel),
					.height = std::max(1u, ktxTexture->baseHeight >> mipLevel),
					.depth = 1
				}
			};
			bufferCopyRegions.push_back(bufferCopyRegion);
		}
	}

	// Create optimal tiled target image
	const VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = aFormat,
		.extent = {.width = aTexture.mWidth, .height = aTexture.mHeight, .depth = 1 },
		.mipLevels = aTexture.mMipLevels,
		.arrayLayers = aTexture.mLayerCount,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &aTexture.mImage));

	VkMemoryRequirements localMemoryRequirements;
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalVkDevice, aTexture.mImage, &localMemoryRequirements);

	const VkMemoryAllocateInfo localMemoryAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = localMemoryRequirements.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(localMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &localMemoryAllocateInfo, nullptr, &aTexture.mDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalVkDevice, aTexture.mImage, aTexture.mDeviceMemory, 0));

	VkCommandBuffer copyCommandBuffer = mVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	const VkImageSubresourceRange subresourceRange
	{
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = aTexture.mMipLevels,
		.layerCount = aTexture.mLayerCount
	};
	VulkanTools::SetImageLayout(
		copyCommandBuffer,
		aTexture.mImage,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	vkCmdCopyBufferToImage(
		copyCommandBuffer,
		stagingBuffer,
		aTexture.mImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<Core::uint32>(bufferCopyRegions.size()),
		bufferCopyRegions.data());

	VulkanTools::SetImageLayout(
		copyCommandBuffer,
		aTexture.mImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		subresourceRange);

	aTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	mVulkanDevice->FlushCommandBuffer(copyCommandBuffer, mTransferQueue, true);

	vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, stagingBuffer, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory, nullptr);

	ktxTexture_Destroy(ktxTexture);
}

void TextureManager::CreateFromEmbeddedTexture(vkglTF::Image& aImage, vkglTF::Texture& aTexture, VkFormat& aFormat)
{
	// Texture was loaded using STB_Image
	unsigned char* buffer = nullptr;
	VkDeviceSize bufferSize = 0;
	bool deleteBuffer = false;
	if (aImage.component == 3)
	{
		// Most devices don't support RGB only on Vulkan so convert if necessary
		// TODO: Check actual format support and transform only if required
		bufferSize = static_cast<VkDeviceSize>(aImage.width * aImage.height * 4);
		buffer = new unsigned char[bufferSize];
		unsigned char* rgba = buffer;
		unsigned char* rgb = &aImage.image[0];
		const Core::size size = static_cast<Core::size>(aImage.width * aImage.height);
		for (Core::size i = 0; i < size; ++i)
		{
			for (Core::int32 j = 0; j < 3; ++j)
			{
				rgba[j] = rgb[j];
			}
			rgba += 4;
			rgb += 3;
		}
		deleteBuffer = true;
	}
	else
	{
		buffer = &aImage.image[0];
		bufferSize = aImage.image.size();
	}

	if (!buffer)
	{
		throw std::runtime_error("Buffer is invalid");
	}

	aFormat = VK_FORMAT_R8G8B8A8_UNORM;

	aTexture.mWidth = aImage.width;
	aTexture.mHeight = aImage.height;
	aTexture.mMipLevels = static_cast<Core::uint32>(std::floor(std::log2(std::max(aTexture.mWidth, aTexture.mHeight))) + 1.0);
	aTexture.mLayerCount = aImage.layers;

	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(mVulkanDevice->mVkPhysicalDevice, aFormat, &formatProperties);
	if (!buffer)
	{
		throw std::runtime_error("Buffer is invalid");
	}

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
	{
		throw std::runtime_error("optimalTilingFeatures is invalid");
	}

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
	{
		throw std::runtime_error("optimalTilingFeatures is invalid");
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	const VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	};
	VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	VkMemoryRequirements hostMemoryRequirements{};
	vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalVkDevice, stagingBuffer, &hostMemoryRequirements);

	const VkMemoryAllocateInfo hostMemoryAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = hostMemoryRequirements.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(hostMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &hostMemoryAllocateInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

	Core::uint8* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory, 0, hostMemoryRequirements.size, 0, (void**)&data));
	std::memcpy(data, buffer, bufferSize);
	vkUnmapMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory);

	const VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = aFormat,
		.extent = {.width = aTexture.mWidth, .height = aTexture.mHeight, .depth = 1 },
		.mipLevels = aTexture.mMipLevels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &aTexture.mImage));

	VkMemoryRequirements localMemoryRequirements{};
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalVkDevice, aTexture.mImage, &localMemoryRequirements);

	const VkMemoryAllocateInfo localMemoryAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = localMemoryRequirements.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(localMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &localMemoryAllocateInfo, nullptr, &aTexture.mDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalVkDevice, aTexture.mImage, aTexture.mDeviceMemory, 0));

	VkCommandBuffer copyCommandBuffer = mVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	const VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};

	{
		const VkImageMemoryBarrier imageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = 0,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = aTexture.mImage,
			.subresourceRange = subresourceRange,
		};
		vkCmdPipelineBarrier(copyCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
	}

	const VkBufferImageCopy bufferCopyRegion{
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1
		},
		.imageExtent = {
			.width = aTexture.mWidth,
			.height = aTexture.mHeight,
			.depth = 1
		}
	};
	vkCmdCopyBufferToImage(copyCommandBuffer, stagingBuffer, aTexture.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

	{
		const VkImageMemoryBarrier imageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.image = aTexture.mImage,
			.subresourceRange = subresourceRange,
		};
		vkCmdPipelineBarrier(copyCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
	}

	mVulkanDevice->FlushCommandBuffer(copyCommandBuffer, mTransferQueue, true);

	vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, stagingBuffer, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory, nullptr);

	// Generate the mip chain (glTF uses jpg and png, so we need to create this manually)
	VkCommandBuffer blitCommandBuffer = mVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	for (Core::uint32 i = 1; i < aTexture.mMipLevels; i++)
	{
		VkImageBlit imageBlit{};
		imageBlit.srcSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = i - 1,
			.layerCount = 1,
		};
		imageBlit.srcOffsets[1] = {
			.x = Core::int32(aTexture.mWidth >> (i - 1)),
			.y = Core::int32(aTexture.mHeight >> (i - 1)),
			.z = 1
		};
		imageBlit.dstSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel = i,
			.layerCount = 1,
		};
		imageBlit.dstOffsets[1] = {
			.x = Core::int32(aTexture.mWidth >> i),
			.y = Core::int32(aTexture.mHeight >> i),
			.z = 1
		};

		const VkImageSubresourceRange mipSubRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = i, .levelCount = 1, .layerCount = 1};

		{
			const VkImageMemoryBarrier imageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.image = aTexture.mImage,
				.subresourceRange = mipSubRange
			};
			vkCmdPipelineBarrier(blitCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
		}

		vkCmdBlitImage(blitCommandBuffer, aTexture.mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, aTexture.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

		{
			const VkImageMemoryBarrier imageMemoryBarrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.image = aTexture.mImage,
				.subresourceRange = mipSubRange
			};
			vkCmdPipelineBarrier(blitCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
		}
	}

	{
		aTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		const VkImageSubresourceRange stageFragmentSubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = aTexture.mMipLevels, .layerCount = 1};
		const VkImageMemoryBarrier imageMemoryBarrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.image = aTexture.mImage,
			.subresourceRange = stageFragmentSubresourceRange
		};
		vkCmdPipelineBarrier(blitCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
	}

	if (deleteBuffer)
	{
		delete[] buffer;
	}

	mVulkanDevice->FlushCommandBuffer(blitCommandBuffer, mTransferQueue, true);
}

void TextureManager::CreateResources(vkglTF::Texture& aTexture, const VkFormat& aFormat)
{
	const VkSamplerAddressMode samplerAddressMode = aTexture.mTextureType == vkglTF::TextureType::Flat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	const VkSamplerCreateInfo samplerCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = samplerAddressMode,
		.addressModeV = samplerAddressMode,
		.addressModeW = samplerAddressMode,
		.mipLodBias = 0.0f,
		.anisotropyEnable = mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy,
		.maxAnisotropy = mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy ? mVulkanDevice->mVkPhysicalDeviceProperties.limits.maxSamplerAnisotropy : 1.0f,
		.compareOp = VK_COMPARE_OP_NEVER,
		.minLod = 0.0f,
		.maxLod = static_cast<float>(aTexture.mMipLevels),
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	};
	VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->mLogicalVkDevice, &samplerCreateInfo, nullptr, &aTexture.mSampler));

	const VkImageViewType imageViewType = aTexture.mTextureType == vkglTF::TextureType::Flat ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	const VkImageViewCreateInfo imageViewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = aTexture.mImage,
		.viewType = imageViewType,
		.format = aFormat,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = aTexture.mMipLevels,
			.baseArrayLayer = 0,
			.layerCount = aTexture.mLayerCount }
	};
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalVkDevice, &imageViewCreateInfo, nullptr, &aTexture.mImageView));

	aTexture.mDescriptorImageInfo.sampler = aTexture.mSampler;
	aTexture.mDescriptorImageInfo.imageView = aTexture.mImageView;
	aTexture.mDescriptorImageInfo.imageLayout = aTexture.imageLayout;
}
