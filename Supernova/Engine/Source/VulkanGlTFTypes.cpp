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
#include <cassert>
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

	Material::Material(VulkanDevice* device)
		: mVulkanDevice(device)
	{
	}


	void vkglTF::Texture::UpdateDescriptor()
	{
		mDescriptorImageInfo.sampler = mSampler;
		mDescriptorImageInfo.imageView = mImageView;
		mDescriptorImageInfo.imageLayout = imageLayout;
	}

	void vkglTF::Texture::Destroy()
	{
		if (mVulkanDevice)
		{
			vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mImageView, nullptr);
			vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mImage, nullptr);
			vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mDeviceMemory, nullptr);
			vkDestroySampler(mVulkanDevice->mLogicalVkDevice, mSampler, nullptr);
		}
	}

	void vkglTF::Texture::FromGlTfImage(tinygltf::Image* aGlTFimage, const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aCopyQueue)
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
			assert(buffer);

			format = VK_FORMAT_R8G8B8A8_UNORM;

			mWidth = aGlTFimage->width;
			mHeight = aGlTFimage->height;
			mMipLevels = static_cast<std::uint32_t>(std::floor(std::log2(std::max(mWidth, mHeight))) + 1.0);

			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(aDevice->mVkPhysicalDevice, format, &formatProperties);
			assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT);
			assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);

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
			memcpy(data, buffer, bufferSize);
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
			assert(createFromNamedFileResult == KTX_SUCCESS);

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
			memcpy(data, ktxTextureData, ktxTextureSize);
			vkUnmapMemory(aDevice->mLogicalVkDevice, stagingMemory);

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
			VulkanTools::setImageLayout(copyCmd, mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
			vkCmdCopyBufferToImage(copyCmd, stagingBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());
			VulkanTools::setImageLayout(copyCmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
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
			.maxLod = (float)mMipLevels,
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

	void vkglTF::Material::CreateDescriptorSet(VkDescriptorPool aDescriptorPool, VkDescriptorSetLayout aDescriptorSetLayout, std::uint32_t aDescriptorBindingFlags)
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = aDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &aDescriptorSetLayout,
		};
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocInfo, &descriptorSet));
		std::vector<VkDescriptorImageInfo> imageDescriptors{};
		std::vector<VkWriteDescriptorSet> writeDescriptorSets{};
		if (aDescriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
		{
			imageDescriptors.push_back(baseColorTexture->mDescriptorImageInfo);
			VkWriteDescriptorSet writeDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSet,
				.dstBinding = static_cast<std::uint32_t>(writeDescriptorSets.size()),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &baseColorTexture->mDescriptorImageInfo
			};
			writeDescriptorSets.push_back(writeDescriptorSet);
		}
		if (normalTexture && aDescriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap)
		{
			imageDescriptors.push_back(normalTexture->mDescriptorImageInfo);
			VkWriteDescriptorSet writeDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSet,
				.dstBinding = static_cast<std::uint32_t>(writeDescriptorSets.size()),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &normalTexture->mDescriptorImageInfo
			};
			writeDescriptorSets.push_back(writeDescriptorSet);
		}
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<std::uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void vkglTF::Primitive::setDimensions(glm::vec3 min, glm::vec3 max)
	{
		dimensions.min = min;
		dimensions.max = max;
		dimensions.size = max - min;
		dimensions.center = (min + max) / 2.0f;
		dimensions.radius = glm::distance(min, max) / 2.0f;
	}

	vkglTF::Mesh::Mesh(VulkanDevice* aDevice, glm::mat4 aMatrix)
	{
		mVulkanDevice = aDevice;
		uniformBlock.matrix = aMatrix;
		VK_CHECK_RESULT(aDevice->CreateBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(uniformBlock),
			&uniformBuffer.buffer,
			&uniformBuffer.memory,
			&uniformBlock));
		VK_CHECK_RESULT(vkMapMemory(aDevice->mLogicalVkDevice, uniformBuffer.memory, 0, sizeof(uniformBlock), 0, &uniformBuffer.mapped));
		uniformBuffer.descriptor = {uniformBuffer.buffer, 0, sizeof(uniformBlock)};
	};

	vkglTF::Mesh::~Mesh()
	{
		vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, uniformBuffer.buffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalVkDevice, uniformBuffer.memory, nullptr);
		for (auto primitive : primitives)
		{
			delete primitive;
		}
	}

	glm::mat4 vkglTF::Node::GetLocalMatrix()
	{
		return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
	}

	glm::mat4 vkglTF::Node::GetMatrix()
	{
		glm::mat4 m = GetLocalMatrix();
		vkglTF::Node* p = parent;
		while (p)
		{
			m = p->GetLocalMatrix() * m;
			p = p->parent;
		}
		return m;
	}

	void vkglTF::Node::update()
	{
		if (mesh)
		{
			glm::mat4 m = GetMatrix();
			if (skin)
			{
				mesh->uniformBlock.matrix = m;
				// Update join matrices
				glm::mat4 inverseTransform = glm::inverse(m);
				for (size_t i = 0; i < skin->joints.size(); i++)
				{
					vkglTF::Node* jointNode = skin->joints[i];
					glm::mat4 jointMat = jointNode->GetMatrix() * skin->inverseBindMatrices[i];
					jointMat = inverseTransform * jointMat;
					mesh->uniformBlock.jointMatrix[i] = jointMat;
				}
				mesh->uniformBlock.jointcount = (float)skin->joints.size();
				memcpy(mesh->uniformBuffer.mapped, &mesh->uniformBlock, sizeof(mesh->uniformBlock));
			}
			else
			{
				memcpy(mesh->uniformBuffer.mapped, &m, sizeof(glm::mat4));
			}
		}

		for (auto& child : children)
		{
			child->update();
		}
	}

	vkglTF::Node::~Node()
	{
		if (mesh)
		{
			delete mesh;
		}
		for (auto& child : children)
		{
			delete child;
		}
	}

	VkVertexInputBindingDescription vkglTF::Vertex::vertexInputBindingDescription;
	std::vector<VkVertexInputAttributeDescription> vkglTF::Vertex::vertexInputAttributeDescriptions;
	VkPipelineVertexInputStateCreateInfo vkglTF::Vertex::pipelineVertexInputStateCreateInfo;

	VkVertexInputBindingDescription vkglTF::Vertex::inputBindingDescription(std::uint32_t binding)
	{
		return VkVertexInputBindingDescription({binding, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX});
	}

	VkVertexInputAttributeDescription vkglTF::Vertex::inputAttributeDescription(std::uint32_t binding, std::uint32_t location, VertexComponent component)
	{
		switch (component)
		{
			case VertexComponent::Position:
				return VkVertexInputAttributeDescription({location, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)});
			case VertexComponent::Normal:
				return VkVertexInputAttributeDescription({location, binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
			case VertexComponent::UV:
				return VkVertexInputAttributeDescription({location, binding, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});
			case VertexComponent::Color:
				return VkVertexInputAttributeDescription({location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color)});
			case VertexComponent::Tangent:
				return VkVertexInputAttributeDescription({location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)});
			case VertexComponent::Joint0:
				return VkVertexInputAttributeDescription({location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, joint0)});
			case VertexComponent::Weight0:
				return VkVertexInputAttributeDescription({location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, weight0)});
			default:
				return VkVertexInputAttributeDescription({});
		}
	}

	std::vector<VkVertexInputAttributeDescription> vkglTF::Vertex::inputAttributeDescriptions(std::uint32_t binding, const std::vector<VertexComponent> components)
	{
		std::vector<VkVertexInputAttributeDescription> result;
		std::uint32_t location = 0;
		for (VertexComponent component : components)
		{
			result.push_back(Vertex::inputAttributeDescription(binding, location, component));
			location++;
		}
		return result;
	}

	/** @brief Returns the default pipeline vertex input state create info structure for the requested vertex components */
	VkPipelineVertexInputStateCreateInfo* vkglTF::Vertex::getPipelineVertexInputState(const std::vector<VertexComponent> components)
	{
		vertexInputBindingDescription = Vertex::inputBindingDescription(0);
		Vertex::vertexInputAttributeDescriptions = Vertex::inputAttributeDescriptions(0, components);
		pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
		pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &Vertex::vertexInputBindingDescription;
		pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(Vertex::vertexInputAttributeDescriptions.size());
		pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = Vertex::vertexInputAttributeDescriptions.data();
		return &pipelineVertexInputStateCreateInfo;
	}
};
