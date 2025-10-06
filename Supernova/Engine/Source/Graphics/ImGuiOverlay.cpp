#include "ImGuiOverlay.hpp"

#include "FileLoader.hpp"
#include "VulkanInitializers.hpp"
#include "VulkanTools.hpp"
#include "VulkanTypes.hpp"

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <glm/vec2.hpp>
#include <imgui.h>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

ImGuiOverlay::ImGuiOverlay()
	: mVulkanDevice{nullptr}
	, mQueue{VK_NULL_HANDLE}
	, mRasterizationSamples{VK_SAMPLE_COUNT_1_BIT}
	, mSubpass{0}
	, gMaxConcurrentFrames{0}
	, mCurrentBufferIndex{0}
	, mDescriptorPool{VK_NULL_HANDLE}
	, mDescriptorSetLayout{VK_NULL_HANDLE}
	, mDescriptorSet{VK_NULL_HANDLE}
	, mPipelineLayout{VK_NULL_HANDLE}
	, mPipeline{VK_NULL_HANDLE}
	, mFontMemory{VK_NULL_HANDLE}
	, mFontImage{VK_NULL_HANDLE}
	, mFontImageView{VK_NULL_HANDLE}
	, mSampler{VK_NULL_HANDLE}
	, mIsVisible{true}
	, mScale{1.0f}
{
	// Init ImGui
	ImGui::CreateContext();
	// Color scheme
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
	style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
	// Dimensions
	ImGuiIO& io = ImGui::GetIO();
	io.FontGlobalScale = mScale;
}

ImGuiOverlay::~ImGuiOverlay()
{
	if (ImGui::GetCurrentContext())
	{
		ImGui::DestroyContext();
	}
}

/** Prepare all vulkan resources required to render the UI overlay */
void ImGuiOverlay::PrepareResources()
{
	assert(gMaxConcurrentFrames > 0);

	ImGuiIO& io = ImGui::GetIO();

	// Create font texture
	unsigned char* fontData;
	int texWidth, texHeight;
	const std::filesystem::path fontPath = "Roboto-Medium.ttf";
	const std::filesystem::path filePath = FileLoader::GetEngineResourcesPath() / FileLoader::gFontPath / fontPath;
	io.Fonts->AddFontFromFileTTF(filePath.generic_string().c_str(), 16.0f * mScale);
	io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
	VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

	// Set ImGui style scale factor to handle retina and other HiDPI displays (same as font scaling above)
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(mScale);

	// Create target image for copy
	VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {.width = (std::uint32_t)texWidth, .height = (std::uint32_t)texHeight, .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalVkDevice, &imageInfo, nullptr, &mFontImage));
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalVkDevice, mFontImage, &memReqs);
	VkMemoryAllocateInfo memAllocInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memReqs.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &mFontMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalVkDevice, mFontImage, mFontMemory, 0));

	// Image view
	VkImageViewCreateInfo viewInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = mFontImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
	};
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalVkDevice, &viewInfo, nullptr, &mFontImageView));

	// Staging buffers for font data upload
	VulkanBuffer stagingBuffer;
	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, uploadSize));
	stagingBuffer.Map();
	std::memcpy(stagingBuffer.mMappedData, fontData, uploadSize);

	// Copy buffer data to font image
	VkCommandBuffer copyCmd = mVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	// Prepare for transfer
	VulkanTools::SetImageLayout(
		copyCmd,
		mFontImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT);
	// Copy
	VkBufferImageCopy bufferCopyRegion{
		.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
		.imageExtent = {.width = (std::uint32_t)texWidth, .height = (std::uint32_t)texHeight, .depth = 1 }
	};
	vkCmdCopyBufferToImage(copyCmd, stagingBuffer.mVkBuffer, mFontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
	// Prepare for shader read
	VulkanTools::SetImageLayout(
		copyCmd,
		mFontImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	mVulkanDevice->flushCommandBuffer(copyCmd, mQueue, true);

	stagingBuffer.Destroy();

	// Font texture Sampler
	VkSamplerCreateInfo samplerInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.maxAnisotropy = 1.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	};
	VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->mLogicalVkDevice, &samplerInfo, nullptr, &mSampler));

	// Descriptor pool
	VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1};
	VkDescriptorPoolCreateInfo descriptorPoolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 2, .poolSizeCount = 1, .pPoolSizes = &poolSize};
	VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalVkDevice, &descriptorPoolInfo, nullptr, &mDescriptorPool));

	// Descriptor set layout
	VkDescriptorSetLayoutBinding setLayoutBinding = VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	VkDescriptorSetLayoutCreateInfo descriptorLayout{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &setLayoutBinding};
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, &descriptorLayout, nullptr, &mDescriptorSetLayout));

	// Descriptor set
	VkDescriptorSetAllocateInfo allocInfo = VulkanInitializers::descriptorSetAllocateInfo(mDescriptorPool, &mDescriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &allocInfo, &mDescriptorSet));
	VkDescriptorImageInfo fontDescriptor{.sampler = mSampler, .imageView = mFontImageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	VkWriteDescriptorSet writeDescriptorSets = VulkanInitializers::writeDescriptorSet(mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor);
	vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, 1, &writeDescriptorSets, 0, nullptr);

	// Buffers per max. frames-in-flight
	mBuffers.resize(gMaxConcurrentFrames);
}

/** Prepare a separate pipeline for the UI overlay rendering decoupled from the main application */
void ImGuiOverlay::PreparePipeline(const VkPipelineCache aPipelineCache, const VkFormat aColorFormat, const VkFormat aDepthFormat)
{
	// Pipeline layout
	// Push constants for UI rendering parameters
	VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(PushConstBlock)};
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &mDescriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalVkDevice, &pipelineLayoutCreateInfo, nullptr, &mPipelineLayout));

	// Setup graphics pipeline for UI rendering
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = VulkanInitializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationState = VulkanInitializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

	// Enable blending
	VkPipelineColorBlendAttachmentState blendAttachmentState{
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	// Vertex bindings an attributes based on ImGui vertex definition
	std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		{.binding = 0, .stride = sizeof(ImDrawVert), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX }
	};
	std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		{.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(ImDrawVert, pos) },
		{.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(ImDrawVert, uv) },
		{.location = 2, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UNORM, .offset = offsetof(ImDrawVert, col) },
	};
	VkPipelineVertexInputStateCreateInfo vertexInputState = VulkanInitializers::pipelineVertexInputStateCreateInfo();
	vertexInputState.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vertexInputBindings.size());
	vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputState.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertexInputAttributes.size());
	vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

	VkPipelineColorBlendStateCreateInfo colorBlendState = VulkanInitializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = VulkanInitializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
	VkPipelineViewportStateCreateInfo viewportState = VulkanInitializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleState = VulkanInitializers::pipelineMultisampleStateCreateInfo(mRasterizationSamples);
	std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState = VulkanInitializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
	VkGraphicsPipelineCreateInfo pipelineCreateInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = static_cast<std::uint32_t>(mShaders.size()),
		.pStages = mShaders.data(),
		.pVertexInputState = &vertexInputState,
		.pInputAssemblyState = &inputAssemblyState,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizationState,
		.pMultisampleState = &multisampleState,
		.pDepthStencilState = &depthStencilState,
		.pColorBlendState = &colorBlendState,
		.pDynamicState = &dynamicState,
		.layout = mPipelineLayout,
		.subpass = mSubpass,
	};

	// If we are using dynamic rendering (renderPass is null), we must define color, depth and stencil attachments at pipeline create time
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &aColorFormat,
			.depthAttachmentFormat = aDepthFormat,
			.stencilAttachmentFormat = aDepthFormat
	};
	pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, aPipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipeline));
}

/** Update vertex and index buffer containing the imGui elements when required */
void ImGuiOverlay::Update(std::uint32_t aCurrentBufferIndex)
{
	ImDrawData* imDrawData = ImGui::GetDrawData();

	if (!imDrawData)
	{
		return;
	}

	// Note: Alignment is done inside buffer creation
	VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
	VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

	// Update buffers only if vertex or index count has been changed compared to current buffer size
	if ((vertexBufferSize == 0) || (indexBufferSize == 0))
	{
		return;
	}

	// Create buffers with multiple of a chunk size to minimize the need to recreate them
	const VkDeviceSize chunkSize = 16384;
	vertexBufferSize = ((vertexBufferSize + chunkSize - 1) / chunkSize) * chunkSize;
	indexBufferSize = ((indexBufferSize + chunkSize - 1) / chunkSize) * chunkSize;

	// Recreate vertex buffer only if necessary
	if ((mBuffers[aCurrentBufferIndex].vertexBuffer.mVkBuffer == VK_NULL_HANDLE) || (mBuffers[aCurrentBufferIndex].vertexBuffer.mVkDeviceSize < vertexBufferSize))
	{
		mBuffers[aCurrentBufferIndex].vertexBuffer.Unmap();
		mBuffers[aCurrentBufferIndex].vertexBuffer.Destroy();
		VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mBuffers[aCurrentBufferIndex].vertexBuffer, vertexBufferSize));
		mBuffers[aCurrentBufferIndex].vertexCount = imDrawData->TotalVtxCount;
		mBuffers[aCurrentBufferIndex].vertexBuffer.Map();
	}

	// Recreate index buffer only if necessary
	if ((mBuffers[aCurrentBufferIndex].indexBuffer.mVkBuffer == VK_NULL_HANDLE) || (mBuffers[aCurrentBufferIndex].indexBuffer.mVkDeviceSize < indexBufferSize))
	{
		mBuffers[aCurrentBufferIndex].indexBuffer.Unmap();
		mBuffers[aCurrentBufferIndex].indexBuffer.Destroy();
		VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mBuffers[aCurrentBufferIndex].indexBuffer, indexBufferSize));
		mBuffers[aCurrentBufferIndex].indexCount = imDrawData->TotalIdxCount;
		mBuffers[aCurrentBufferIndex].indexBuffer.Map();
	}

	// Upload data
	ImDrawVert* vtxDst = (ImDrawVert*)mBuffers[aCurrentBufferIndex].vertexBuffer.mMappedData;
	ImDrawIdx* idxDst = (ImDrawIdx*)mBuffers[aCurrentBufferIndex].indexBuffer.mMappedData;

	for (int n = 0; n < imDrawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = imDrawData->CmdLists[n];
		std::memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		std::memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtxDst += cmd_list->VtxBuffer.Size;
		idxDst += cmd_list->IdxBuffer.Size;
	}

	// Flush to make writes visible to GPU
	mBuffers[aCurrentBufferIndex].vertexBuffer.Flush();
	mBuffers[aCurrentBufferIndex].indexBuffer.Flush();
}

void ImGuiOverlay::Draw(const VkCommandBuffer aVkCommandBuffer, std::uint32_t aCurrentBufferIndex)
{
	ImDrawData* imDrawData = ImGui::GetDrawData();
	std::int32_t vertexOffset = 0;
	std::int32_t indexOffset = 0;

	if ((!imDrawData) || (imDrawData->CmdListsCount == 0))
	{
		return;
	}

	if (mBuffers[aCurrentBufferIndex].vertexBuffer.mVkBuffer == VK_NULL_HANDLE || mBuffers[aCurrentBufferIndex].indexBuffer.mVkBuffer == VK_NULL_HANDLE)
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	vkCmdBindPipeline(aVkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
	vkCmdBindDescriptorSets(aVkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSet, 0, NULL);

	mPushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
	mPushConstBlock.translate = glm::vec2(-1.0f);
	vkCmdPushConstants(aVkCommandBuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &mPushConstBlock);

	assert(mBuffers[aCurrentBufferIndex].vertexBuffer.mVkBuffer != VK_NULL_HANDLE && mBuffers[aCurrentBufferIndex].indexBuffer.mVkBuffer != VK_NULL_HANDLE);

	VkDeviceSize offsets[1] = {0};
	vkCmdBindVertexBuffers(aVkCommandBuffer, 0, 1, &mBuffers[aCurrentBufferIndex].vertexBuffer.mVkBuffer, offsets);
	vkCmdBindIndexBuffer(aVkCommandBuffer, mBuffers[aCurrentBufferIndex].indexBuffer.mVkBuffer, 0, VK_INDEX_TYPE_UINT16);

	for (std::int32_t i = 0; i < imDrawData->CmdListsCount; i++)
	{
		const ImDrawList* cmd_list = imDrawData->CmdLists[i];
		for (std::int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
			VkRect2D scissorRect{
				.offset = {.x = std::max((std::int32_t)(pcmd->ClipRect.x), 0), .y = std::max((std::int32_t)(pcmd->ClipRect.y), 0) },
				.extent = {.width = (std::uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x), .height = (std::uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y) }
			};
			vkCmdSetScissor(aVkCommandBuffer, 0, 1, &scissorRect);
			vkCmdDrawIndexed(aVkCommandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
			indexOffset += pcmd->ElemCount;
		}
		vertexOffset += cmd_list->VtxBuffer.Size;
	}
}

void ImGuiOverlay::Resize(std::uint32_t aWidth, std::uint32_t aHeight)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)(aWidth), (float)(aHeight));
}

void ImGuiOverlay::FreeResources()
{
	for (Buffers& buffer : mBuffers)
	{
		buffer.vertexBuffer.Destroy();
		buffer.indexBuffer.Destroy();
	}

	vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mFontImageView, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mFontImage, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mFontMemory, nullptr);
	vkDestroySampler(mVulkanDevice->mLogicalVkDevice, mSampler, nullptr);
	vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, mDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(mVulkanDevice->mLogicalVkDevice, mDescriptorPool, nullptr);
	vkDestroyPipelineLayout(mVulkanDevice->mLogicalVkDevice, mPipelineLayout, nullptr);
	vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mPipeline, nullptr);
}

bool ImGuiOverlay::header(const char* caption)
{
	return ImGui::CollapsingHeader(caption, ImGuiTreeNodeFlags_DefaultOpen);
}

bool ImGuiOverlay::checkBox(const char* caption, bool* value)
{
	return ImGui::Checkbox(caption, value);
}

bool ImGuiOverlay::checkBox(const char* caption, std::int32_t* value)
{
	bool val = (*value == 1);
	bool res = ImGui::Checkbox(caption, &val);
	*value = val;
	return res;
}

bool ImGuiOverlay::radioButton(const char* caption, bool value)
{
	return ImGui::RadioButton(caption, value);
}

bool ImGuiOverlay::inputFloat(const char* caption, float* value, float step, const char* format)
{
	return ImGui::InputFloat(caption, value, step, step * 10.0f, format);
}

bool ImGuiOverlay::sliderFloat(const char* caption, float* value, float min, float max)
{
	return ImGui::SliderFloat(caption, value, min, max);
}

bool ImGuiOverlay::sliderInt(const char* caption, std::int32_t* value, std::int32_t min, std::int32_t max)
{
	return ImGui::SliderInt(caption, value, min, max);
}

bool ImGuiOverlay::comboBox(const char* caption, std::int32_t* itemindex, std::vector<std::string> items)
{
	if (items.empty())
	{
		return false;
	}
	std::vector<const char*> charitems;
	charitems.reserve(items.size());
	for (size_t i = 0; i < items.size(); i++)
	{
		charitems.push_back(items[i].c_str());
	}
	std::uint32_t itemCount = static_cast<std::uint32_t>(charitems.size());
	return ImGui::Combo(caption, itemindex, &charitems[0], itemCount, itemCount);
}

bool ImGuiOverlay::button(const char* caption)
{
	return ImGui::Button(caption);
}

bool ImGuiOverlay::colorPicker(const char* caption, float* color)
{
	return ImGui::ColorEdit4(caption, color, ImGuiColorEditFlags_NoInputs);
}

void ImGuiOverlay::text(const char* formatstr, ...)
{
	va_list args;
	va_start(args, formatstr);
	ImGui::TextV(formatstr, args);
	va_end(args);
}
