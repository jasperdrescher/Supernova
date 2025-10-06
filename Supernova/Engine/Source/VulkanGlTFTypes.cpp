#include "VulkanGlTFTypes.hpp"

#include "FileLoader.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTools.hpp"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <vulkan/vulkan_core.h>
#include <cstdint>
#include <format>
#include <ktx.h>
#include <ktxvulkan.h>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace vkglTF
{
	Texture::Texture()
		: mVulkanDevice{nullptr}
		, mImage{VK_NULL_HANDLE}
		, mDeviceMemory{VK_NULL_HANDLE}
		, mImageView{VK_NULL_HANDLE}
		, mWidth{0}
		, mHeight{0}
		, mMipLevels{0}
		, mLayerCount{0}
		, mSampler{VK_NULL_HANDLE}
		, mIndex{0}
	{
	}

	Material::Material(VulkanDevice* aDevice)
		: mVulkanDevice{aDevice}
		, mAlphaMode{AlphaMode::ALPHAMODE_OPAQUE}
		, mAlphaCutoff{1.0f}
		, mMetallicFactor{1.0f}
		, mRoughnessFactor{1.0f}
		, mBaseColorFactor{1.0f}
		, mBaseColorTexture{nullptr}
		, mMetallicRoughnessTexture{nullptr}
		, mNormalTexture{nullptr}
		, mOcclusionTexture{nullptr}
		, mEmissiveTexture{nullptr}
		, mSpecularGlossinessTexture{nullptr}
		, mDiffuseTexture{nullptr}
		, mDescriptorSet{VK_NULL_HANDLE}
	{
	}

	void Texture::UpdateDescriptor()
	{
		mDescriptorImageInfo.sampler = mSampler;
		mDescriptorImageInfo.imageView = mImageView;
		mDescriptorImageInfo.imageLayout = imageLayout;
	}

	void Texture::Destroy()
	{
		if (mVulkanDevice)
		{
			vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mImageView, nullptr);
			vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mImage, nullptr);
			vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mDeviceMemory, nullptr);
			vkDestroySampler(mVulkanDevice->mLogicalVkDevice, mSampler, nullptr);
		}
	}

	void Texture::FromGlTfImage(tinygltf::Image* aGlTFimage, const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aCopyQueue)
	{
		mVulkanDevice = aDevice;

		bool isKtx = false;
		// Image points to an external ktx file
		if (aGlTFimage->uri.find_last_of(".") != std::string::npos)
		{
			if (aGlTFimage->uri.substr(aGlTFimage->uri.find_last_of(".") + 1) == "ktx")
			{
				isKtx = true;
			}
		}

		VkFormat format;

		if (!isKtx)
		{
			// Texture was loaded using STB_Image

			unsigned char* buffer = nullptr;
			VkDeviceSize bufferSize = 0;
			bool deleteBuffer = false;
			if (aGlTFimage->component == 3)
			{
				// Most devices don't support RGB only on Vulkan so convert if necessary
				// TODO: Check actual format support and transform only if required
				bufferSize = aGlTFimage->width * aGlTFimage->height * 4;
				buffer = new unsigned char[bufferSize];
				unsigned char* rgba = buffer;
				unsigned char* rgb = &aGlTFimage->image[0];
				for (size_t i = 0; i < aGlTFimage->width * aGlTFimage->height; ++i)
				{
					for (int32_t j = 0; j < 3; ++j)
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
				buffer = &aGlTFimage->image[0];
				bufferSize = aGlTFimage->image.size();
			}
			
			if (!buffer)
			{
				throw std::runtime_error("Buffer is invalid");
			}

			format = VK_FORMAT_R8G8B8A8_UNORM;

			mWidth = aGlTFimage->width;
			mHeight = aGlTFimage->height;
			mMipLevels = static_cast<std::uint32_t>(std::floor(std::log2(std::max(mWidth, mHeight))) + 1.0);

			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(aDevice->mVkPhysicalDevice, format, &formatProperties);
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

			VkBufferCreateInfo bufferCreateInfo{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = bufferSize,
				.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
			};
			VK_CHECK_RESULT(vkCreateBuffer(aDevice->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));
			VkMemoryRequirements memReqs{};
			vkGetBufferMemoryRequirements(aDevice->mLogicalVkDevice, stagingBuffer, &memReqs);
			VkMemoryAllocateInfo memAllocInfo{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = memReqs.size,
				.memoryTypeIndex = aDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
			};
			VK_CHECK_RESULT(vkAllocateMemory(aDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &stagingMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(aDevice->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

			std::uint8_t* data{nullptr};
			VK_CHECK_RESULT(vkMapMemory(aDevice->mLogicalVkDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
			std::memcpy(data, buffer, bufferSize);
			vkUnmapMemory(aDevice->mLogicalVkDevice, stagingMemory);

			VkImageCreateInfo imageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = format,
				.extent = {.width = mWidth, .height = mHeight, .depth = 1 },
				.mipLevels = mMipLevels,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};
			VK_CHECK_RESULT(vkCreateImage(aDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &mImage));
			vkGetImageMemoryRequirements(aDevice->mLogicalVkDevice, mImage, &memReqs);
			memAllocInfo.allocationSize = memReqs.size;
			memAllocInfo.memoryTypeIndex = aDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(aDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &mDeviceMemory));
			VK_CHECK_RESULT(vkBindImageMemory(aDevice->mLogicalVkDevice, mImage, mDeviceMemory, 0));

			VkCommandBuffer copyCmd = aDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
			{
				VkImageMemoryBarrier imageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = 0,
					.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.image = mImage,
					.subresourceRange = subresourceRange,
				};
				vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
			}
			VkBufferImageCopy bufferCopyRegion{
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1
				},
				.imageExtent = {
					.width = mWidth,
					.height = mHeight,
					.depth = 1
				}
			};
			vkCmdCopyBufferToImage(copyCmd, stagingBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
			{
				VkImageMemoryBarrier imageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					.image = mImage,
					.subresourceRange = subresourceRange,
				};
				vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
			}
			aDevice->flushCommandBuffer(copyCmd, aCopyQueue, true);

			vkDestroyBuffer(aDevice->mLogicalVkDevice, stagingBuffer, nullptr);
			vkFreeMemory(aDevice->mLogicalVkDevice, stagingMemory, nullptr);

			// Generate the mip chain (glTF uses jpg and png, so we need to create this manually)
			VkCommandBuffer blitCmd = aDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			for (std::uint32_t i = 1; i < mMipLevels; i++)
			{
				VkImageBlit imageBlit{};
				imageBlit.srcSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = i - 1,
					.layerCount = 1,
				};
				imageBlit.srcOffsets[1] = {
					.x = int32_t(mWidth >> (i - 1)),
					.y = int32_t(mHeight >> (i - 1)),
					.z = 1
				};
				imageBlit.dstSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = i,
					.layerCount = 1,
				};
				imageBlit.dstOffsets[1] = {
					.x = int32_t(mWidth >> i),
					.y = int32_t(mHeight >> i),
					.z = 1
				};

				VkImageSubresourceRange mipSubRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = i, .levelCount = 1, .layerCount = 1};
				{
					VkImageMemoryBarrier imageMemoryBarrier{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.srcAccessMask = 0,
						.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.image = mImage,
						.subresourceRange = mipSubRange
					};
					vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
				}
				vkCmdBlitImage(blitCmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
				{
					VkImageMemoryBarrier imageMemoryBarrier{
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
						.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
						.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
						.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						.image = mImage,
						.subresourceRange = mipSubRange
					};
					vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
				}
			}

			subresourceRange.levelCount = mMipLevels;
			imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			{
				VkImageMemoryBarrier imageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.image = mImage,
					.subresourceRange = subresourceRange
				};
				vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
			}
			if (deleteBuffer)
			{
				delete[] buffer;
			}

			aDevice->flushCommandBuffer(blitCmd, aCopyQueue, true);
		}
		else
		{
			// Texture is stored in an external ktx file
			const std::filesystem::path externalPath = aPath / aGlTFimage->uri;

			ktxTexture* ktxTexture;

			if (!FileLoader::IsFileValid(externalPath))
			{
				throw std::runtime_error(std::format("Could not load texture from: {}", externalPath.generic_string()));
			}

			const ktxResult createFromNamedFileResult = ktxTexture_CreateFromNamedFile(externalPath.generic_string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
			if (createFromNamedFileResult != KTX_SUCCESS)
			{
				throw std::runtime_error(std::format("Could not create named file: {}", externalPath.generic_string()));
			}

			mVulkanDevice = aDevice;
			mWidth = ktxTexture->baseWidth;
			mHeight = ktxTexture->baseHeight;
			mMipLevels = ktxTexture->numLevels;

			ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
			ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);
			format = ktxTexture_GetVkFormat(ktxTexture);

			// Get device properties for the requested texture format
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(aDevice->mVkPhysicalDevice, format, &formatProperties);

			VkCommandBuffer copyCmd = aDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingMemory;

			VkBufferCreateInfo bufferCreateInfo{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = ktxTextureSize,
				.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE
			};
			VK_CHECK_RESULT(vkCreateBuffer(aDevice->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

			VkMemoryRequirements memReqs;
			vkGetBufferMemoryRequirements(aDevice->mLogicalVkDevice, stagingBuffer, &memReqs);
			VkMemoryAllocateInfo memAllocInfo{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.allocationSize = memReqs.size,
				.memoryTypeIndex = aDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
			};
			VK_CHECK_RESULT(vkAllocateMemory(aDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &stagingMemory));
			VK_CHECK_RESULT(vkBindBufferMemory(aDevice->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

			uint8_t* data{nullptr};
			VK_CHECK_RESULT(vkMapMemory(aDevice->mLogicalVkDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
			std::memcpy(data, ktxTextureData, ktxTextureSize);
			vkUnmapMemory(aDevice->mLogicalVkDevice, stagingMemory);

			std::vector<VkBufferImageCopy> bufferCopyRegions;
			for (std::uint32_t i = 0; i < mMipLevels; i++)
			{
				ktx_size_t offset;
				const KTX_error_code getImageOffsetResult = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
				if (!getImageOffsetResult != KTX_SUCCESS)
				{
					throw std::runtime_error("Could not get image offset");
				}

				VkBufferImageCopy bufferCopyRegion{
					.bufferOffset = offset,
					.imageSubresource = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = i,
						.baseArrayLayer = 0,
						.layerCount = 1
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
				.extent = {.width = mWidth, .height = mHeight, .depth = 1 },
				.mipLevels = mMipLevels,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = VK_IMAGE_TILING_OPTIMAL,
				.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			};
			VK_CHECK_RESULT(vkCreateImage(aDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &mImage));

			vkGetImageMemoryRequirements(aDevice->mLogicalVkDevice, mImage, &memReqs);
			memAllocInfo.allocationSize = memReqs.size;
			memAllocInfo.memoryTypeIndex = aDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(aDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &mDeviceMemory));
			VK_CHECK_RESULT(vkBindImageMemory(aDevice->mLogicalVkDevice, mImage, mDeviceMemory, 0));

			VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = mMipLevels, .layerCount = 1};
			VulkanTools::SetImageLayout(copyCmd, mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
			vkCmdCopyBufferToImage(copyCmd, stagingBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());
			VulkanTools::SetImageLayout(copyCmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
			aDevice->flushCommandBuffer(copyCmd, aCopyQueue, true);
			imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			vkDestroyBuffer(aDevice->mLogicalVkDevice, stagingBuffer, nullptr);
			vkFreeMemory(aDevice->mLogicalVkDevice, stagingMemory, nullptr);

			ktxTexture_Destroy(ktxTexture);
		}

		VkSamplerCreateInfo samplerInfo{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
			.anisotropyEnable = VK_TRUE,
			.maxAnisotropy = 8.0f,
			.compareOp = VK_COMPARE_OP_NEVER,
			.maxLod = static_cast<float>(mMipLevels),
			.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		};
		VK_CHECK_RESULT(vkCreateSampler(aDevice->mLogicalVkDevice, &samplerInfo, nullptr, &mSampler));

		VkImageViewCreateInfo viewInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = mImage,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format,
			.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = mMipLevels, .layerCount = 1 }
		};
		VK_CHECK_RESULT(vkCreateImageView(aDevice->mLogicalVkDevice, &viewInfo, nullptr, &mImageView));

		mDescriptorImageInfo.sampler = mSampler;
		mDescriptorImageInfo.imageView = mImageView;
		mDescriptorImageInfo.imageLayout = imageLayout;
	}

	void Material::CreateDescriptorSet(VkDescriptorPool aDescriptorPool, VkDescriptorSetLayout aDescriptorSetLayout, std::uint32_t aDescriptorBindingFlags)
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = aDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &aDescriptorSetLayout,
		};
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocInfo, &mDescriptorSet));
		std::vector<VkDescriptorImageInfo> imageDescriptors{};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets{};
		if (aDescriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
		{
			imageDescriptors.push_back(mBaseColorTexture->mDescriptorImageInfo);
			VkWriteDescriptorSet writeDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = mDescriptorSet,
				.dstBinding = static_cast<std::uint32_t>(writeDescriptorSets.size()),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &mBaseColorTexture->mDescriptorImageInfo
			};
			writeDescriptorSets.push_back(writeDescriptorSet);
		}
		if (mNormalTexture && aDescriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap)
		{
			imageDescriptors.push_back(mNormalTexture->mDescriptorImageInfo);
			VkWriteDescriptorSet writeDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = mDescriptorSet,
				.dstBinding = static_cast<std::uint32_t>(writeDescriptorSets.size()),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &mNormalTexture->mDescriptorImageInfo
			};
			writeDescriptorSets.push_back(writeDescriptorSet);
		}
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<std::uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void vkglTF::Primitive::SetDimensions(const glm::vec3& aMin, const glm::vec3& aMax)
	{
		mDimensions.mMin = aMin;
		mDimensions.mMax = aMax;
		mDimensions.mSize = aMax - aMin;
		mDimensions.mCenter = (aMin + aMax) / 2.0f;
		mDimensions.mRadius = glm::distance(aMin, aMax) / 2.0f;
	}

	vkglTF::Mesh::Mesh(VulkanDevice* aDevice, const glm::mat4& aMatrix)
	{
		mVulkanDevice = aDevice;
		mUniformBlock.mMatrix = aMatrix;
		VK_CHECK_RESULT(aDevice->CreateBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(mUniformBlock),
			&mUniformBuffer.buffer,
			&mUniformBuffer.memory,
			&mUniformBlock));
		VK_CHECK_RESULT(vkMapMemory(aDevice->mLogicalVkDevice, mUniformBuffer.memory, 0, sizeof(mUniformBlock), 0, &mUniformBuffer.mMappedData));
		mUniformBuffer.descriptor = {mUniformBuffer.buffer, 0, sizeof(mUniformBlock)};
	};

	Mesh::~Mesh()
	{
		vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, mUniformBuffer.buffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mUniformBuffer.memory, nullptr);
		for (vkglTF::Primitive* primitive : mPrimitives)
		{
			delete primitive;
		}
	}

	glm::mat4 Node::GetLocalMatrix() const
	{
		return glm::translate(glm::mat4(1.0f), mTranslation) * glm::mat4(mRotation) * glm::scale(glm::mat4(1.0f), mScale) * mMatrix;
	}

	glm::mat4 Node::GetMatrix() const
	{
		glm::mat4 m = GetLocalMatrix();
		vkglTF::Node* p = mParent;
		while (p)
		{
			m = p->GetLocalMatrix() * m;
			p = p->mParent;
		}
		return m;
	}

	void Node::update()
	{
		if (mMesh)
		{
			glm::mat4 m = GetMatrix();
			if (mSkin)
			{
				mMesh->mUniformBlock.mMatrix = m;
				// Update join matrices
				glm::mat4 inverseTransform = glm::inverse(m);
				for (size_t i = 0; i < mSkin->joints.size(); i++)
				{
					vkglTF::Node* jointNode = mSkin->joints[i];
					glm::mat4 jointMat = jointNode->GetMatrix() * mSkin->inverseBindMatrices[i];
					jointMat = inverseTransform * jointMat;
					mMesh->mUniformBlock.mJointMatrix[i] = jointMat;
				}
				mMesh->mUniformBlock.mJointcount = static_cast<float>(mSkin->joints.size());
				std::memcpy(mMesh->mUniformBuffer.mMappedData, &mMesh->mUniformBlock, sizeof(mMesh->mUniformBlock));
			}
			else
			{
				std::memcpy(mMesh->mUniformBuffer.mMappedData, &m, sizeof(glm::mat4));
			}
		}

		for (vkglTF::Node* child : mChildren)
		{
			child->update();
		}
	}

	Node::~Node()
	{
		if (mMesh)
		{
			delete mMesh;
		}

		for (vkglTF::Node* child : mChildren)
		{
			delete child;
		}
	}

	VkVertexInputBindingDescription Vertex::mVertexInputBindingDescription;
	std::vector<VkVertexInputAttributeDescription> Vertex::mVertexInputAttributeDescriptions;
	VkPipelineVertexInputStateCreateInfo Vertex::mPipelineVertexInputStateCreateInfo;

	VkVertexInputBindingDescription vkglTF::Vertex::inputBindingDescription(std::uint32_t aBinding)
	{
		return VkVertexInputBindingDescription({aBinding, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX});
	}

	VkVertexInputAttributeDescription vkglTF::Vertex::inputAttributeDescription(std::uint32_t aBinding, std::uint32_t aLocation, VertexComponent aComponent)
	{
		switch (aComponent)
		{
			case VertexComponent::Position:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, mPosition)});
			case VertexComponent::Normal:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, mNormal)});
			case VertexComponent::UV:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, mUV)});
			case VertexComponent::Color:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, mColor)});
			case VertexComponent::Tangent:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, mTangent)});
			case VertexComponent::Joint0:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, mJoint0)});
			case VertexComponent::Weight0:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, mWeight0)});
			default:
				return VkVertexInputAttributeDescription({});
		}
	}

	std::vector<VkVertexInputAttributeDescription> vkglTF::Vertex::inputAttributeDescriptions(std::uint32_t aBinding, const std::vector<VertexComponent>& aComponents)
	{
		std::vector<VkVertexInputAttributeDescription> result;
		std::uint32_t location = 0;
		for (VertexComponent component : aComponents)
		{
			result.push_back(Vertex::inputAttributeDescription(aBinding, location, component));
			location++;
		}
		return result;
	}

	/** @brief Returns the default pipeline vertex input state create info structure for the requested vertex components */
	VkPipelineVertexInputStateCreateInfo* vkglTF::Vertex::getPipelineVertexInputState(const std::vector<VertexComponent>& aComponents)
	{
		mVertexInputBindingDescription = Vertex::inputBindingDescription(0);
		Vertex::mVertexInputAttributeDescriptions = Vertex::inputAttributeDescriptions(0, aComponents);
		mPipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		mPipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
		mPipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &Vertex::mVertexInputBindingDescription;
		mPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(Vertex::mVertexInputAttributeDescriptions.size());
		mPipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = Vertex::mVertexInputAttributeDescriptions.data();
		return &mPipelineVertexInputStateCreateInfo;
	}
};
