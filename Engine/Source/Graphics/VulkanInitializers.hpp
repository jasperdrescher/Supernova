#pragma once

#include "Core/Types.hpp"

#include <vector>
#include <vulkan/vulkan_core.h>

namespace VulkanInitializers
{
	inline VkCommandBufferAllocateInfo CommandBufferAllocateInfo(
		VkCommandPool aCommandPool,
		VkCommandBufferLevel aCommandBufferLevel,
		Core::uint32 aBufferCount)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.commandPool = aCommandPool;
		commandBufferAllocateInfo.level = aCommandBufferLevel;
		commandBufferAllocateInfo.commandBufferCount = aBufferCount;
		return commandBufferAllocateInfo;
	}

	inline VkImageMemoryBarrier ImageMemoryBarrier()
	{
		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		return imageMemoryBarrier;
	}
	
	inline VkCommandBufferBeginInfo CommandBufferBeginInfo()
	{
		VkCommandBufferBeginInfo commandBufferBeginInfo{};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		return commandBufferBeginInfo;
	}

	inline VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags aFlags = 0)
	{
		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = aFlags;
		return fenceCreateInfo;
	}
	
	inline VkBufferCreateInfo BufferCreateInfo(
		VkBufferUsageFlags aUsageFlags,
		VkDeviceSize aDeviceSize)
	{
		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.usage = aUsageFlags;
		bufferCreateInfo.size = aDeviceSize;
		return bufferCreateInfo;
	}

	inline VkMemoryAllocateInfo MemoryAllocateInfo()
	{
		VkMemoryAllocateInfo memoryAllocInfo{};
		memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		return memoryAllocInfo;
	}

	inline VkDescriptorPoolSize DescriptorPoolSize(
		VkDescriptorType aDescriptorType,
		Core::uint32 aDescriptorCount)
	{
		VkDescriptorPoolSize descriptorPoolSize{};
		descriptorPoolSize.type = aDescriptorType;
		descriptorPoolSize.descriptorCount = aDescriptorCount;
		return descriptorPoolSize;
	}

	inline VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(
		const std::vector<VkDescriptorPoolSize>& aDescriptorPoolSizes,
		Core::uint32 aMaxSets)
	{
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<Core::uint32>(aDescriptorPoolSizes.size());
		descriptorPoolInfo.pPoolSizes = aDescriptorPoolSizes.data();
		descriptorPoolInfo.maxSets = aMaxSets;
		return descriptorPoolInfo;
	}

	inline VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(
		VkDescriptorType aDescriptorType,
		VkShaderStageFlags aShaderStageFlags,
		Core::uint32 aBindings,
		Core::uint32 aDescriptorCount = 1)
	{
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		setLayoutBinding.descriptorType = aDescriptorType;
		setLayoutBinding.stageFlags = aShaderStageFlags;
		setLayoutBinding.binding = aBindings;
		setLayoutBinding.descriptorCount = aDescriptorCount;
		return setLayoutBinding;
	}

	inline VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(
		const std::vector<VkDescriptorSetLayoutBinding>& aBindings)
	{
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.pBindings = aBindings.data();
		descriptorSetLayoutCreateInfo.bindingCount = static_cast<Core::uint32>(aBindings.size());
		return descriptorSetLayoutCreateInfo;
	}

	inline VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(
		VkDescriptorPool aDescriptorPool,
		const VkDescriptorSetLayout* aDescriptorSetLayouts,
		Core::uint32 aDescriptorSetCount)
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = aDescriptorPool;
		descriptorSetAllocateInfo.pSetLayouts = aDescriptorSetLayouts;
		descriptorSetAllocateInfo.descriptorSetCount = aDescriptorSetCount;
		return descriptorSetAllocateInfo;
	}

	inline VkWriteDescriptorSet WriteDescriptorSet(
		VkDescriptorSet aDestinationDescriptorSet,
		VkDescriptorType aDescriptorType,
		Core::uint32 aBinding,
		const VkDescriptorBufferInfo* aDescriptorBufferInfo,
		Core::uint32 aDescriptorCount = 1)
	{
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = aDestinationDescriptorSet;
		writeDescriptorSet.descriptorType = aDescriptorType;
		writeDescriptorSet.dstBinding = aBinding;
		writeDescriptorSet.pBufferInfo = aDescriptorBufferInfo;
		writeDescriptorSet.descriptorCount = aDescriptorCount;
		return writeDescriptorSet;
	}

	inline VkWriteDescriptorSet WriteDescriptorSet(
		VkDescriptorSet aDestinationDescriptorSet,
		VkDescriptorType aDescriptorType,
		Core::uint32 aBinding,
		const VkDescriptorImageInfo* aDescriptorImageInfo,
		Core::uint32 aDescriptorCount = 1)
	{
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = aDestinationDescriptorSet;
		writeDescriptorSet.descriptorType = aDescriptorType;
		writeDescriptorSet.dstBinding = aBinding;
		writeDescriptorSet.pImageInfo = aDescriptorImageInfo;
		writeDescriptorSet.descriptorCount = aDescriptorCount;
		return writeDescriptorSet;
	}

	inline VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo(
		const VkDescriptorSetLayout* aDescriptorSetLayouts,
		Core::uint32 aSetLayoutCount = 1)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = aSetLayoutCount;
		pipelineLayoutCreateInfo.pSetLayouts = aDescriptorSetLayouts;
		return pipelineLayoutCreateInfo;
	}

	inline VkGraphicsPipelineCreateInfo PipelineCreateInfo()
	{
		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.basePipelineIndex = -1;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		return pipelineCreateInfo;
	}

	inline VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo(
		VkPrimitiveTopology aTopology,
		VkPipelineInputAssemblyStateCreateFlags aFlags,
		VkBool32 aPrimitiveRestartEnable)
	{
		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
		pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		pipelineInputAssemblyStateCreateInfo.topology = aTopology;
		pipelineInputAssemblyStateCreateInfo.flags = aFlags;
		pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = aPrimitiveRestartEnable;
		return pipelineInputAssemblyStateCreateInfo;
	}

	inline VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(
		VkPolygonMode aPolygonMode,
		VkCullModeFlags aCullMode,
		VkFrontFace aFrontFace,
		VkPipelineRasterizationStateCreateFlags aFlags = 0)
	{
		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
		pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		pipelineRasterizationStateCreateInfo.polygonMode = aPolygonMode;
		pipelineRasterizationStateCreateInfo.cullMode = aCullMode;
		pipelineRasterizationStateCreateInfo.frontFace = aFrontFace;
		pipelineRasterizationStateCreateInfo.flags = aFlags;
		pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
		return pipelineRasterizationStateCreateInfo;
	}

	inline VkPipelineColorBlendAttachmentState PipelineColorBlendAttachmentState(
		VkColorComponentFlags aColorWriteMask,
		VkBool32 aBlendEnable)
	{
		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
		pipelineColorBlendAttachmentState.colorWriteMask = aColorWriteMask;
		pipelineColorBlendAttachmentState.blendEnable = aBlendEnable;
		return pipelineColorBlendAttachmentState;
	}

	inline VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo(
		Core::uint32 aAttachmentCount,
		const VkPipelineColorBlendAttachmentState* aAttachments)
	{
		VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
		pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		pipelineColorBlendStateCreateInfo.attachmentCount = aAttachmentCount;
		pipelineColorBlendStateCreateInfo.pAttachments = aAttachments;
		return pipelineColorBlendStateCreateInfo;
	}

	inline VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilStateCreateInfo(VkBool32 aDepthTestEnable, VkBool32 aDepthWriteEnable, VkCompareOp aDepthCompareOp)
	{
		VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
		pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		pipelineDepthStencilStateCreateInfo.depthTestEnable = aDepthTestEnable;
		pipelineDepthStencilStateCreateInfo.depthWriteEnable = aDepthWriteEnable;
		pipelineDepthStencilStateCreateInfo.depthCompareOp = aDepthCompareOp;
		pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
		return pipelineDepthStencilStateCreateInfo;
	}

	inline VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo(
		Core::uint32 aViewportCount,
		Core::uint32 aScissorCount,
		VkPipelineViewportStateCreateFlags aFlags = 0)
	{
		VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
		pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		pipelineViewportStateCreateInfo.viewportCount = aViewportCount;
		pipelineViewportStateCreateInfo.scissorCount = aScissorCount;
		pipelineViewportStateCreateInfo.flags = aFlags;
		return pipelineViewportStateCreateInfo;
	}

	inline VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo(
		VkSampleCountFlagBits aRasterizationSamples,
		VkPipelineMultisampleStateCreateFlags aFlags = 0)
	{
		VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
		pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		pipelineMultisampleStateCreateInfo.rasterizationSamples = aRasterizationSamples;
		pipelineMultisampleStateCreateInfo.flags = aFlags;
		return pipelineMultisampleStateCreateInfo;
	}

	inline VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(
		const std::vector<VkDynamicState>& aDynamicStates,
		VkPipelineDynamicStateCreateFlags aFlags = 0)
	{
		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
		pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		pipelineDynamicStateCreateInfo.pDynamicStates = aDynamicStates.data();
		pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<Core::uint32>(aDynamicStates.size());
		pipelineDynamicStateCreateInfo.flags = aFlags;
		return pipelineDynamicStateCreateInfo;
	}

	inline VkViewport Viewport(
		float aWidth,
		float aHeight,
		float aMinDepth,
		float aMaxDepth)
	{
		VkViewport viewport{};
		viewport.width = aWidth;
		viewport.height = aHeight;
		viewport.minDepth = aMinDepth;
		viewport.maxDepth = aMaxDepth;
		return viewport;
	}

	inline VkRect2D Rect2D(
		Core::uint32 aWidth,
		Core::uint32 aHeight,
		Core::int32 aOffsetX,
		Core::int32 aOffsetY)
	{
		VkRect2D rect2D{};
		rect2D.extent.width = aWidth;
		rect2D.extent.height = aHeight;
		rect2D.offset.x = aOffsetX;
		rect2D.offset.y = aOffsetY;
		return rect2D;
	}

	inline VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo()
	{
		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
		pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		return pipelineVertexInputStateCreateInfo;
	}

	inline VkVertexInputBindingDescription VertexInputBindingDescription(
		Core::uint32 aBinding,
		Core::uint32 aStride,
		VkVertexInputRate aInputRate)
	{
		VkVertexInputBindingDescription vertexinputBindDescription{};
		vertexinputBindDescription.binding = aBinding;
		vertexinputBindDescription.stride = aStride;
		vertexinputBindDescription.inputRate = aInputRate;
		return vertexinputBindDescription;
	}

	inline VkVertexInputAttributeDescription VertexInputAttributeDescription(
		Core::uint32 aBinding,
		Core::uint32 aLocation,
		VkFormat aFormat,
		Core::uint32 aOffset)
	{
		VkVertexInputAttributeDescription vertexInputAttribDescription{};
		vertexInputAttribDescription.location = aLocation;
		vertexInputAttribDescription.binding = aBinding;
		vertexInputAttribDescription.format = aFormat;
		vertexInputAttribDescription.offset = aOffset;
		return vertexInputAttribDescription;
	}

	inline VkComputePipelineCreateInfo ComputePipelineCreateInfo(
		VkPipelineLayout aLayout,
		VkPipelineCreateFlags aFlags = 0)
	{
		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = aLayout;
		computePipelineCreateInfo.flags = aFlags;
		return computePipelineCreateInfo;
	}
}
