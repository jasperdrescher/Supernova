#include "VulkanRenderer.hpp"

#include "VulkanDebug.hpp"
#include "VulkanInitializers.hpp"
#include "VulkanTools.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <ratio>
#include <chrono>

namespace VulkanExampleLocal
{
	static void GLFWErrorCallback(int error, const char* description)
	{
		std::cerr << "GLFW error: " << error << " " << description << std::endl;
	}

	static std::vector<const char*> GetGlfwRequiredExtensions()
	{
		std::uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		if (glfwExtensionCount == 0)
			throw std::runtime_error("Failed to find required GLFW extensions");

		std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		return requiredExtensions;
	}
}

VulkanRenderer::VulkanRenderer()
	: mGlfwWindow(nullptr)
	, mShouldClose(false)
	, mIsFramebufferResized(false)
	, mVkCommandBuffers{VK_NULL_HANDLE}
{
	mVulkanApplicationProperties.mAPIVersion = VK_API_VERSION_1_3;
	mVulkanApplicationProperties.mIsValidationEnabled = true;
	mVulkanApplicationProperties.mApplicationName = "Supernova";
	mVulkanApplicationProperties.mEngineName = "Supernova";

	mFramebufferWidth = mVulkanApplicationProperties.mWindowWidth;
	mFramebufferHeight = mVulkanApplicationProperties.mWindowHeight;

	mVkPhysicalDevice13Features.dynamicRendering = VK_TRUE;
	mVkPhysicalDevice13Features.synchronization2 = VK_TRUE;

	// Setup a default look-at camera
	camera.type = Camera::CameraType::lookat;
	camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
	camera.setRotation(glm::vec3(0.0f));
	camera.setPerspective(60.0f, static_cast<float>(mFramebufferWidth) / static_cast<float>(mFramebufferHeight), 1.0f, 256.0f);
}

VulkanRenderer::~VulkanRenderer()
{
	mVulkanSwapChain.cleanup();
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(mVkLogicalDevice, descriptorPool, nullptr);
	}
	destroyCommandBuffers();
	if (renderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(mVkLogicalDevice, renderPass, nullptr);
	}

	for (auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(mVkLogicalDevice, shaderModule, nullptr);
	}
	vkDestroyImageView(mVkLogicalDevice, depthStencil.mVkImageView, nullptr);
	vkDestroyImage(mVkLogicalDevice, depthStencil.mVkImage, nullptr);
	vkFreeMemory(mVkLogicalDevice, depthStencil.mVkDeviceMemory, nullptr);

	vkDestroyPipelineCache(mVkLogicalDevice, pipelineCache, nullptr);

	vkDestroyCommandPool(mVkLogicalDevice, mVkCommandPool, nullptr);

	for (auto& fence : waitFences)
	{
		vkDestroyFence(mVkLogicalDevice, fence, nullptr);
	}

	for (auto& semaphore : presentCompleteSemaphores)
	{
		vkDestroySemaphore(mVkLogicalDevice, semaphore, nullptr);
	}
	for (auto& semaphore : renderCompleteSemaphores)
	{
		vkDestroySemaphore(mVkLogicalDevice, semaphore, nullptr);
	}

	delete vulkanDevice;

	if (mVulkanApplicationProperties.mIsValidationEnabled)
	{
		vks::debug::freeDebugCallback(mVkInstance);
	}

	vkDestroyInstance(mVkInstance, nullptr);

	if (mVkLogicalDevice)
	{
		vkDestroyPipeline(mVkLogicalDevice, pipeline, nullptr);
		vkDestroyPipelineLayout(mVkLogicalDevice, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVkLogicalDevice, descriptorSetLayout, nullptr);
		vkDestroyBuffer(mVkLogicalDevice, vertexBuffer.mVkBuffer, nullptr);
		vkFreeMemory(mVkLogicalDevice, vertexBuffer.mVkDeviceMemory, nullptr);
		vkDestroyBuffer(mVkLogicalDevice, indexBuffer.mVkBuffer, nullptr);
		vkFreeMemory(mVkLogicalDevice, indexBuffer.mVkDeviceMemory, nullptr);
		vkDestroyCommandPool(mVkLogicalDevice, commandPool, nullptr);
		for (size_t i = 0; i < presentCompleteSemaphores.size(); i++)
		{
			vkDestroySemaphore(mVkLogicalDevice, presentCompleteSemaphores[i], nullptr);
		}
		for (size_t i = 0; i < renderCompleteSemaphores.size(); i++)
		{
			vkDestroySemaphore(mVkLogicalDevice, renderCompleteSemaphores[i], nullptr);
		}
		for (std::uint32_t i = 0; i < gMaxConcurrentFrames; i++)
		{
			vkDestroyFence(mVkLogicalDevice, waitFences[i], nullptr);
			vkDestroyBuffer(mVkLogicalDevice, uniformBuffers[i].mVkBuffer, nullptr);
			vkFreeMemory(mVkLogicalDevice, uniformBuffers[i].mVkDeviceMemory, nullptr);
		}
	}
}

void VulkanRenderer::InitializeRenderer()
{
	CreateGlfwWindow();
	initVulkan();
	PrepareVulkanResources();
}

void VulkanRenderer::PrepareUpdate()
{
	mLastTimestamp = std::chrono::high_resolution_clock::now();
	mPreviousEndTime = mLastTimestamp;
}

void VulkanRenderer::UpdateRenderer(float aDeltaTime)
{
	if (mIsPrepared)
	{
		NextFrame();
	}

	camera.update(mFrameTime);

	if (mVkLogicalDevice != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(mVkLogicalDevice);
	}

	glfwSetWindowTitle(mGlfwWindow, GetWindowTitle(aDeltaTime).c_str());
	glfwPollEvents();

	mVulkanApplicationProperties.mIsFocused = glfwGetWindowAttrib(mGlfwWindow, GLFW_FOCUSED);
	mShouldClose = glfwWindowShouldClose(mGlfwWindow);
}

void VulkanRenderer::DestroyRenderer()
{
	glfwDestroyWindow(mGlfwWindow);
	glfwTerminate();
}

void VulkanRenderer::GetEnabledFeatures() const
{
	// Vulkan 1.3 device support is required for this example
	if (deviceProperties.apiVersion < VK_API_VERSION_1_3)
	{
		vks::tools::exitFatal("Selected GPU does not support support Vulkan 1.3", VK_ERROR_INCOMPATIBLE_DRIVER);
	}
}

std::uint32_t VulkanRenderer::getMemoryTypeIndex(std::uint32_t typeBits, VkMemoryPropertyFlags properties)
{
	// Iterate over all memory types available for the device used in this example
	for (std::uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		typeBits >>= 1;
	}
	throw "Could not find a suitable memory type!";
}

void VulkanRenderer::createSynchronizationPrimitives()
{
	// Fences are per frame in flight
	for (std::uint32_t i = 0; i < gMaxConcurrentFrames; i++)
	{
		// Fence used to ensure that command buffer has completed exection before using it again
		VkFenceCreateInfo fenceCI{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		// Create the fences in signaled state (so we don't wait on first render of each command buffer)
		fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		VK_CHECK_RESULT(vkCreateFence(mVkLogicalDevice, &fenceCI, nullptr, &waitFences[i]));
	}
	// Semaphores are used for correct command ordering within a queue
	// Used to ensure that image presentation is complete before starting to submit again
	presentCompleteSemaphores.resize(gMaxConcurrentFrames);
	for (auto& semaphore : presentCompleteSemaphores)
	{
		VkSemaphoreCreateInfo semaphoreCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VK_CHECK_RESULT(vkCreateSemaphore(mVkLogicalDevice, &semaphoreCI, nullptr, &semaphore));
	}
	// Render completion
	// Semaphore used to ensure that all commands submitted have been finished before submitting the image to the queue
	renderCompleteSemaphores.resize(mVulkanSwapChain.mVkImages.size());
	for (auto& semaphore : renderCompleteSemaphores)
	{
		VkSemaphoreCreateInfo semaphoreCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VK_CHECK_RESULT(vkCreateSemaphore(mVkLogicalDevice, &semaphoreCI, nullptr, &semaphore));
	}
}

// Command buffers are used to record commands to and are submitted to a queue for execution ("rendering")
void VulkanRenderer::createCommandBuffers()
{
	// All command buffers are allocated from the same command pool
	VkCommandPoolCreateInfo commandPoolCI{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	commandPoolCI.queueFamilyIndex = mVulkanSwapChain.mQueueNodeIndex;
	commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(mVkLogicalDevice, &commandPoolCI, nullptr, &commandPool));
	// Allocate one command buffer per max. concurrent frame from above pool
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, gMaxConcurrentFrames);
	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVkLogicalDevice, &cmdBufAllocateInfo, commandBuffers.data()));
}

void VulkanRenderer::createVertexBuffer()
{
	const std::vector<VulkanVertex> vertices{
		{{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
		{{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		{{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
	};
	std::uint32_t vertexBufferSize = static_cast<std::uint32_t>(vertices.size()) * sizeof(VulkanVertex);

	// Setup indices
	// We do this for demonstration purposes, a triangle doesn't require indices to be rendered (because of the 1:1 mapping), but more complex shapes usually make use of indices
	std::vector<std::uint32_t> indices{0, 1, 2};
	indexCount = static_cast<std::uint32_t>(indices.size());
	std::uint32_t indexBufferSize = indexCount * sizeof(std::uint32_t);

	VkMemoryAllocateInfo memAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	VkMemoryRequirements memReqs;

	// Static data like vertex and index buffer should be stored on the device memory for optimal (and fastest) access by the GPU
	//
	// To achieve this we use so-called "staging buffers" :
	// - Create a buffer that's visible to the host (and can be mapped)
	// - Copy the data to this buffer
	// - Create another buffer that's local on the device (VRAM) with the same size
	// - Copy the data from the host to the device using a command buffer
	// - Delete the host visible (staging) buffer
	// - Use the device local buffers for rendering
	//
	// Note: On unified memory architectures where host (CPU) and GPU share the same memory, staging is not necessary
	// To keep this sample easy to follow, there is no check for that in place

	// Create the host visible staging buffer that we copy vertices and indices too, and from which we copy to the device
	VulkanBuffer stagingBuffer;
	VkBufferCreateInfo stagingBufferCI{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	stagingBufferCI.size = vertexBufferSize + indexBufferSize;
	// Buffer is used as the copy source
	stagingBufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	// Create a host-visible buffer to copy the vertex data to (staging buffer)
	VK_CHECK_RESULT(vkCreateBuffer(mVkLogicalDevice, &stagingBufferCI, nullptr, &stagingBuffer.mVkBuffer));
	vkGetBufferMemoryRequirements(mVkLogicalDevice, stagingBuffer.mVkBuffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	// Request a host visible memory type that can be used to copy our data to
	// Also request it to be coherent, so that writes are visible to the GPU right after unmapping the buffer
	memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVkLogicalDevice, &memAlloc, nullptr, &stagingBuffer.mVkDeviceMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVkLogicalDevice, stagingBuffer.mVkBuffer, stagingBuffer.mVkDeviceMemory, 0));
	// Map the buffer and copy vertices and indices into it, this way we can use a single buffer as the source for both vertex and index GPU buffers
	uint8_t* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(mVkLogicalDevice, stagingBuffer.mVkDeviceMemory, 0, memAlloc.allocationSize, 0, (void**)&data));
	memcpy(data, vertices.data(), vertexBufferSize);
	memcpy(((char*)data) + vertexBufferSize, indices.data(), indexBufferSize);

	// Create a device local buffer to which the (host local) vertex data will be copied and which will be used for rendering
	VkBufferCreateInfo vertexbufferCI{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	vertexbufferCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vertexbufferCI.size = vertexBufferSize;
	VK_CHECK_RESULT(vkCreateBuffer(mVkLogicalDevice, &vertexbufferCI, nullptr, &vertexBuffer.mVkBuffer));
	vkGetBufferMemoryRequirements(mVkLogicalDevice, vertexBuffer.mVkBuffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVkLogicalDevice, &memAlloc, nullptr, &vertexBuffer.mVkDeviceMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVkLogicalDevice, vertexBuffer.mVkBuffer, vertexBuffer.mVkDeviceMemory, 0));

	// Create a device local buffer to which the (host local) index data will be copied and which will be used for rendering
	VkBufferCreateInfo indexbufferCI{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	indexbufferCI.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	indexbufferCI.size = indexBufferSize;
	VK_CHECK_RESULT(vkCreateBuffer(mVkLogicalDevice, &indexbufferCI, nullptr, &indexBuffer.mVkBuffer));
	vkGetBufferMemoryRequirements(mVkLogicalDevice, indexBuffer.mVkBuffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVkLogicalDevice, &memAlloc, nullptr, &indexBuffer.mVkDeviceMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVkLogicalDevice, indexBuffer.mVkBuffer, indexBuffer.mVkDeviceMemory, 0));

	// Buffer copies have to be submitted to a queue, so we need a command buffer for them
	VkCommandBuffer copyCmd;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	cmdBufAllocateInfo.commandPool = commandPool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;
	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVkLogicalDevice, &cmdBufAllocateInfo, &copyCmd));

	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
	VK_CHECK_RESULT(vkBeginCommandBuffer(copyCmd, &cmdBufInfo));
	// Copy vertex and index buffers to the device
	VkBufferCopy copyRegion{};
	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(copyCmd, stagingBuffer.mVkBuffer, vertexBuffer.mVkBuffer, 1, &copyRegion);
	copyRegion.size = indexBufferSize;
	// Indices are stored after the vertices in the source buffer, so we need to add an offset
	copyRegion.srcOffset = vertexBufferSize;
	vkCmdCopyBuffer(copyCmd, stagingBuffer.mVkBuffer, indexBuffer.mVkBuffer, 1, &copyRegion);
	VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

	// Submit the command buffer to the queue to finish the copy
	VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &copyCmd;

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceCI{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	VkFence fence;
	VK_CHECK_RESULT(vkCreateFence(mVkLogicalDevice, &fenceCI, nullptr, &fence));
	// Submit copies to the queue
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
	// Wait for the fence to signal that command buffer has finished executing
	VK_CHECK_RESULT(vkWaitForFences(mVkLogicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
	vkDestroyFence(mVkLogicalDevice, fence, nullptr);
	vkFreeCommandBuffers(mVkLogicalDevice, commandPool, 1, &copyCmd);

	// The fence made sure copies are finished, so we can safely delete the staging buffer
	vkDestroyBuffer(mVkLogicalDevice, stagingBuffer.mVkBuffer, nullptr);
	vkFreeMemory(mVkLogicalDevice, stagingBuffer.mVkDeviceMemory, nullptr);
}

void VulkanRenderer::createDescriptors()
{
	// Descriptors are allocated from a pool, that tells the implementation how many and what types of descriptors we are going to use (at maximum)
	VkDescriptorPoolSize descriptorTypeCounts[1]{};
	// This example only one descriptor type (uniform buffer)
	descriptorTypeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	// We have one buffer (and as such descriptor) per frame
	descriptorTypeCounts[0].descriptorCount = gMaxConcurrentFrames;
	// For additional types you need to add new entries in the type count list
	// E.g. for two combined image samplers :
	// typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	// typeCounts[1].descriptorCount = 2;

	// Create the global descriptor pool
	// All descriptors used in this example are allocated from this pool
	VkDescriptorPoolCreateInfo descriptorPoolCI{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	descriptorPoolCI.poolSizeCount = 1;
	descriptorPoolCI.pPoolSizes = descriptorTypeCounts;
	// Set the max. number of descriptor sets that can be requested from this pool (requesting beyond this limit will result in an error)
	// Our sample will create one set per uniform buffer per frame
	descriptorPoolCI.maxSets = gMaxConcurrentFrames;
	VK_CHECK_RESULT(vkCreateDescriptorPool(mVkLogicalDevice, &descriptorPoolCI, nullptr, &descriptorPool));

	// Descriptor set layouts define the interface between our application and the shader
	// Basically connects the different shader stages to descriptors for binding uniform buffers, image samplers, etc.
	// So every shader binding should map to one descriptor set layout binding
	// Binding 0: Uniform buffer (Vertex shader)
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	descriptorLayoutCI.bindingCount = 1;
	descriptorLayoutCI.pBindings = &layoutBinding;
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVkLogicalDevice, &descriptorLayoutCI, nullptr, &descriptorSetLayout));

	// Where the descriptor set layout is the interface, the descriptor set points to actual data
	// Descriptors that are changed per frame need to be multiplied, so we can update descriptor n+1 while n is still used by the GPU, so we create one per max frame in flight
	for (std::uint32_t i = 0; i < gMaxConcurrentFrames; i++)
	{
		VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVkLogicalDevice, &allocInfo, &uniformBuffers[i].mVkDescriptorSet));

		// Update the descriptor set determining the shader binding points
		// For every binding point used in a shader there needs to be one
		// descriptor set matching that binding point
		VkWriteDescriptorSet writeDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

		// The buffer's information is passed using a descriptor info structure
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i].mVkBuffer;
		bufferInfo.range = sizeof(VulkanShaderData);

		// Binding 0 : Uniform buffer
		writeDescriptorSet.dstSet = uniformBuffers[i].mVkDescriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &bufferInfo;
		writeDescriptorSet.dstBinding = 0;
		vkUpdateDescriptorSets(mVkLogicalDevice, 1, &writeDescriptorSet, 0, nullptr);
	}
}

void VulkanRenderer::setupDepthStencil()
{
	// Create an optimal tiled image used as the depth stencil attachment
	VkImageCreateInfo imageCI{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = depthFormat;
	imageCI.extent = {mFramebufferWidth, mFramebufferHeight, 1};
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK_RESULT(vkCreateImage(mVkLogicalDevice, &imageCI, nullptr, &depthStencil.mVkImage));

	// Allocate memory for the image (device local) and bind it to our image
	VkMemoryAllocateInfo memAlloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(mVkLogicalDevice, depthStencil.mVkImage, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVkLogicalDevice, &memAlloc, nullptr, &depthStencil.mVkDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVkLogicalDevice, depthStencil.mVkImage, depthStencil.mVkDeviceMemory, 0));

	// Create a view for the depth stencil image
	// Images aren't directly accessed in Vulkan, but rather through views described by a subresource range
	// This allows for multiple views of one image with differing ranges (e.g. for different layers)
	VkImageViewCreateInfo depthStencilViewCI{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	depthStencilViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilViewCI.format = depthFormat;
	depthStencilViewCI.subresourceRange = {};
	depthStencilViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT)
	if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
	{
		depthStencilViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	depthStencilViewCI.subresourceRange.baseMipLevel = 0;
	depthStencilViewCI.subresourceRange.levelCount = 1;
	depthStencilViewCI.subresourceRange.baseArrayLayer = 0;
	depthStencilViewCI.subresourceRange.layerCount = 1;
	depthStencilViewCI.image = depthStencil.mVkImage;
	VK_CHECK_RESULT(vkCreateImageView(mVkLogicalDevice, &depthStencilViewCI, nullptr, &depthStencil.mVkImageView));
}

VkShaderModule VulkanRenderer::loadSPIRVShader(const std::string& filename)
{
	size_t shaderSize;
	char* shaderCode{nullptr};

	std::ifstream is(filename, std::ios::binary | std::ios::in | std::ios::ate);

	if (is.is_open())
	{
		shaderSize = is.tellg();
		is.seekg(0, std::ios::beg);
		// Copy file contents into a buffer
		shaderCode = new char[shaderSize];
		is.read(shaderCode, shaderSize);
		is.close();
		assert(shaderSize > 0);
	}

	if (shaderCode)
	{
		// Create a new shader module that will be used for pipeline creation
		VkShaderModuleCreateInfo shaderModuleCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		shaderModuleCI.codeSize = shaderSize;
		shaderModuleCI.pCode = (std::uint32_t*)shaderCode;

		VkShaderModule shaderModule;
		VK_CHECK_RESULT(vkCreateShaderModule(mVkLogicalDevice, &shaderModuleCI, nullptr, &shaderModule));

		delete[] shaderCode;

		return shaderModule;
	}
	else
	{
		std::cerr << "Error: Could not open shader file \"" << filename << "\"" << std::endl;
		return VK_NULL_HANDLE;
	}
}

void VulkanRenderer::createPipeline()
{
	// The pipeline layout is the interface telling the pipeline what type of descriptors will later be bound
	VkPipelineLayoutCreateInfo pipelineLayoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pipelineLayoutCI.setLayoutCount = 1;
	pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVkLogicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

	// Create the graphics pipeline used in this example
	// Vulkan uses the concept of rendering pipelines to encapsulate fixed states, replacing OpenGL's complex state machine
	// A pipeline is then stored and hashed on the GPU making pipeline changes very fast

	VkGraphicsPipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	// The layout used for this pipeline (can be shared among multiple pipelines using the same layout)
	pipelineCI.layout = pipelineLayout;

	// Construct the different states making up the pipeline

	// Input assembly state describes how primitives are assembled
	// This pipeline will assemble vertex data as a triangle lists (though we only use one triangle)
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Rasterization state
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCI.depthClampEnable = VK_FALSE;
	rasterizationStateCI.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCI.depthBiasEnable = VK_FALSE;
	rasterizationStateCI.lineWidth = 1.0f;

	// Color blend state describes how blend factors are calculated (if used)
	// We need one blend attachment state per color attachment (even if blending is not used)
	VkPipelineColorBlendAttachmentState blendAttachmentState{};
	blendAttachmentState.colorWriteMask = 0xf;
	blendAttachmentState.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	colorBlendStateCI.attachmentCount = 1;
	colorBlendStateCI.pAttachments = &blendAttachmentState;

	// Viewport state sets the number of viewports and scissor used in this pipeline
	// Note: This is actually overridden by the dynamic states (see below)
	VkPipelineViewportStateCreateInfo viewportStateCI{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	viewportStateCI.viewportCount = 1;
	viewportStateCI.scissorCount = 1;

	// Enable dynamic states
	// Most states are baked into the pipeline, but there is somee state that can be dynamically changed within the command buffer to mak e things easuer
	// To be able to change these we need do specify which dynamic states will be changed using this pipeline. Their actual states are set later on in the command buffer
	std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicStateCI{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	dynamicStateCI.pDynamicStates = dynamicStateEnables.data();
	dynamicStateCI.dynamicStateCount = static_cast<std::uint32_t>(dynamicStateEnables.size());

	// Depth and stencil state containing depth and stencil compare and test operations
	// We only use depth tests and want depth tests and writes to be enabled and compare with less or equal
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	depthStencilStateCI.depthTestEnable = VK_TRUE;
	depthStencilStateCI.depthWriteEnable = VK_TRUE;
	depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCI.back.failOp = VK_STENCIL_OP_KEEP;
	depthStencilStateCI.back.passOp = VK_STENCIL_OP_KEEP;
	depthStencilStateCI.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilStateCI.stencilTestEnable = VK_FALSE;
	depthStencilStateCI.front = depthStencilStateCI.back;

	// This example does not make use of multi sampling (for anti-aliasing), the state must still be set and passed to the pipeline
	VkPipelineMultisampleStateCreateInfo multisampleStateCI{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	multisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Vertex input descriptions
	// Specifies the vertex input parameters for a pipeline

	// Vertex input binding
	// This example uses a single vertex input binding at binding point 0 (see vkCmdBindVertexBuffers)
	VkVertexInputBindingDescription vertexInputBinding{};
	vertexInputBinding.binding = 0;
	vertexInputBinding.stride = sizeof(VulkanVertex);
	vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// Input attribute bindings describe shader attribute locations and memory layouts
	std::array<VkVertexInputAttributeDescription, 2> vertexInputAttributs{};
	// These match the following shader layout (see triangle.vert):
	//	layout (location = 0) in vec3 inPos;
	//	layout (location = 1) in vec3 inColor;
	// Attribute location 0: Position
	vertexInputAttributs[0].binding = 0;
	vertexInputAttributs[0].location = 0;
	// Position attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
	vertexInputAttributs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexInputAttributs[0].offset = offsetof(VulkanVertex, mVertexPosition);
	// Attribute location 1: Color
	vertexInputAttributs[1].binding = 0;
	vertexInputAttributs[1].location = 1;
	// Color attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
	vertexInputAttributs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexInputAttributs[1].offset = offsetof(VulkanVertex, mVertexColor);

	// Vertex input state used for pipeline creation
	VkPipelineVertexInputStateCreateInfo vertexInputStateCI{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vertexInputStateCI.vertexBindingDescriptionCount = 1;
	vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
	vertexInputStateCI.vertexAttributeDescriptionCount = 2;
	vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributs.data();

	// Shaders
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

	// Vertex shader
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = loadSPIRVShader(getShadersPath() + "triangle/triangle.vert.spv");
	shaderStages[0].pName = "main";
	assert(shaderStages[0].module != VK_NULL_HANDLE);

	// Fragment shader
	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = loadSPIRVShader(getShadersPath() + "triangle/triangle.frag.spv");
	shaderStages[1].pName = "main";
	assert(shaderStages[1].module != VK_NULL_HANDLE);

	// Set pipeline shader stage info
	pipelineCI.stageCount = static_cast<std::uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	// Attachment information for dynamic rendering
	VkPipelineRenderingCreateInfoKHR pipelineRenderingCI{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
	pipelineRenderingCI.colorAttachmentCount = 1;
	pipelineRenderingCI.pColorAttachmentFormats = &mVulkanSwapChain.mColorVkFormat;
	pipelineRenderingCI.depthAttachmentFormat = depthFormat;
	pipelineRenderingCI.stencilAttachmentFormat = depthFormat;

	// Assign the pipeline states to the pipeline creation info structure
	pipelineCI.pVertexInputState = &vertexInputStateCI;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.pDynamicState = &dynamicStateCI;
	pipelineCI.pNext = &pipelineRenderingCI;

	// Create rendering pipeline using the specified states
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVkLogicalDevice, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

	// Shader modules can safely be destroyed when the pipeline has been created
	vkDestroyShaderModule(mVkLogicalDevice, shaderStages[0].module, nullptr);
	vkDestroyShaderModule(mVkLogicalDevice, shaderStages[1].module, nullptr);
}

void VulkanRenderer::createUniformBuffers()
{
	// Prepare and initialize the per-frame uniform buffer blocks containing shader uniforms
	// Single uniforms like in OpenGL are no longer present in Vulkan. All shader uniforms are passed via uniform buffer blocks
	VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bufferInfo.size = sizeof(VulkanShaderData);
	// This buffer will be used as a uniform buffer
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	// Create the buffers
	for (std::uint32_t i = 0; i < gMaxConcurrentFrames; i++)
	{
		VK_CHECK_RESULT(vkCreateBuffer(mVkLogicalDevice, &bufferInfo, nullptr, &uniformBuffers[i].mVkBuffer));
		// Get memory requirements including size, alignment and memory type based on the buffer type we request (uniform buffer)
		VkMemoryRequirements memReqs;
		vkGetBufferMemoryRequirements(mVkLogicalDevice, uniformBuffers[i].mVkBuffer, &memReqs);
		VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		// Note that we use the size we got from the memory requirements and not the actual buffer size, as the former may be larger due to alignment requirements of the device
		allocInfo.allocationSize = memReqs.size;
		// Get the memory type index that supports host visible memory access
		// Most implementations offer multiple memory types and selecting the correct one to allocate memory from is crucial
		// We also want the buffer to be host coherent so we don't have to flush (or sync after every update).
		allocInfo.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		// Allocate memory for the uniform buffer
		VK_CHECK_RESULT(vkAllocateMemory(mVkLogicalDevice, &allocInfo, nullptr, &(uniformBuffers[i].mVkDeviceMemory)));
		// Bind memory to buffer
		VK_CHECK_RESULT(vkBindBufferMemory(mVkLogicalDevice, uniformBuffers[i].mVkBuffer, uniformBuffers[i].mVkDeviceMemory, 0));
		// We map the buffer once, so we can update it without having to map it again
		VK_CHECK_RESULT(vkMapMemory(mVkLogicalDevice, uniformBuffers[i].mVkDeviceMemory, 0, sizeof(VulkanShaderData), 0, (void**)&uniformBuffers[i].mMappedData));
	}

}

void VulkanRenderer::PrepareVulkanResources()
{
	InitializeSwapchain();
	createCommandPool();
	SetupSwapchain();
	setupDepthStencil();
	createPipelineCache();

	createSynchronizationPrimitives();
	createCommandBuffers();
	createVertexBuffer();
	createUniformBuffers();
	createDescriptors();
	createPipeline();

	mIsPrepared = true;
}

void VulkanRenderer::PrepareFrame()
{
	// Use a fence to wait until the command buffer has finished execution before using it again
	vkWaitForFences(mVkLogicalDevice, 1, &waitFences[currentFrame], VK_TRUE, UINT64_MAX);
	VK_CHECK_RESULT(vkResetFences(mVkLogicalDevice, 1, &waitFences[currentFrame]));

	// Get the next swap chain image from the implementation
	// Note that the implementation is free to return the images in any order, so we must use the acquire function and can't just cycle through the images/imageIndex on our own
	std::uint32_t imageIndex{0};
	VkResult result = vkAcquireNextImageKHR(mVkLogicalDevice, mVulkanSwapChain.mVkSwapchainKHR, UINT64_MAX, presentCompleteSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		OnResizeWindow();
		return;
	}
	else if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR))
	{
		throw "Could not acquire the next swap chain image!";
	}

	// Update the uniform buffer for the next frame
	VulkanShaderData shaderData{};
	shaderData.projectionMatrix = camera.matrices.perspective;
	shaderData.viewMatrix = camera.matrices.view;
	shaderData.modelMatrix = glm::mat4(1.0f);
	// Copy the current matrices to the current frame's uniform buffer. As we requested a host coherent memory type for the uniform buffer, the write is instantly visible to the GPU.
	memcpy(uniformBuffers[currentFrame].mMappedData, &shaderData, sizeof(VulkanShaderData));

	// Build the command buffer for the next frame to render
	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	VkCommandBufferBeginInfo cmdBufInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	const VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
	VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

	// With dynamic rendering we need to explicitly add layout transitions by using barriers, this set of barriers prepares the color and depth images for output
	vks::tools::insertImageMemoryBarrier(commandBuffer, mVulkanSwapChain.mVkImages[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
	vks::tools::insertImageMemoryBarrier(commandBuffer, depthStencil.mVkImage, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1});

	// New structures are used to define the attachments used in dynamic rendering
	// Color attachment
	VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	colorAttachment.imageView = mVulkanSwapChain.mVkImageViews[imageIndex];
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue.color = {0.0f, 0.0f, 0.2f, 0.0f};
	// Depth/stencil attachment
	VkRenderingAttachmentInfo depthStencilAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	depthStencilAttachment.imageView = depthStencil.mVkImageView;
	depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthStencilAttachment.clearValue.depthStencil = {1.0f, 0};

	VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
	renderingInfo.renderArea = {0, 0, mFramebufferWidth, mFramebufferHeight};
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthStencilAttachment;
	renderingInfo.pStencilAttachment = &depthStencilAttachment;

	// Start a dynamic rendering section
	vkCmdBeginRendering(commandBuffer, &renderingInfo);
	// Update dynamic viewport state
	VkViewport viewport{0.0f, 0.0f, static_cast<float>(mFramebufferWidth), static_cast<float>(mFramebufferHeight), 0.0f, 1.0f};
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	// Update dynamic scissor state
	VkRect2D scissor{0, 0, mFramebufferWidth, mFramebufferHeight};
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	// Bind descriptor set for the current frame's uniform buffer, so the shader uses the data from that buffer for this draw
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &uniformBuffers[currentFrame].mVkDescriptorSet, 0, nullptr);
	// The pipeline (state object) contains all states of the rendering pipeline, binding it will set all the states specified at pipeline creation time
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	// Bind triangle vertex buffer (contains position and colors)
	VkDeviceSize offsets[1]{0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.mVkBuffer, offsets);
	// Bind triangle index buffer
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer.mVkBuffer, 0, VK_INDEX_TYPE_UINT32);
	// Draw indexed triangle
	vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
	// Finish the current dynamic rendering section
	vkCmdEndRendering(commandBuffer);

	// This barrier prepares the color image for presentation, we don't need to care for the depth image
	vks::tools::insertImageMemoryBarrier(commandBuffer, mVulkanSwapChain.mVkImages[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_NONE, VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

	// Submit the command buffer to the graphics queue

	// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
	VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// The submit info structure specifies a command buffer queue submission batch
	VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submitInfo.pWaitDstStageMask = &waitStageMask;      // Pointer to the list of pipeline stages that the semaphore waits will occur at
	submitInfo.pCommandBuffers = &commandBuffer;		// Command buffers(s) to execute in this batch (submission)
	submitInfo.commandBufferCount = 1;                  // We submit a single command buffer

	// Semaphore to wait upon before the submitted command buffer starts executing
	submitInfo.pWaitSemaphores = &presentCompleteSemaphores[currentFrame];
	submitInfo.waitSemaphoreCount = 1;
	// Semaphore to be signaled when command buffers have completed
	submitInfo.pSignalSemaphores = &renderCompleteSemaphores[imageIndex];
	submitInfo.signalSemaphoreCount = 1;

	// Submit to the graphics queue passing a wait fence
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, waitFences[currentFrame]));

	// Present the current frame buffer to the swap chain
	// Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
	// This ensures that the image is not presented to the windowing system until all commands have been submitted
	VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderCompleteSemaphores[imageIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &mVulkanSwapChain.mVkSwapchainKHR;
	presentInfo.pImageIndices = &imageIndex;
	result = vkQueuePresentKHR(queue, &presentInfo);
	if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR) || mIsFramebufferResized)
	{
		mIsFramebufferResized = false;

		if (!mVulkanApplicationProperties.mIsMinimized)
			OnResizeWindow();
	}
	else if (result != VK_SUCCESS)
	{
		throw "Could not present the image to the swap chain!";
	}

	// Select the next frame to render to, based on the max. no. of concurrent frames
	currentFrame = (currentFrame + 1) % gMaxConcurrentFrames;
}

void VulkanRenderer::CreateGlfwWindow()
{
	if (!glfwInit())
	{
		throw std::runtime_error("Failed to init GLFW");
	}

	glfwSetErrorCallback(VulkanExampleLocal::GLFWErrorCallback);

	if (!glfwVulkanSupported())
	{
		throw std::runtime_error("Failed to init Vulkan");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_SAMPLES, 4);
	mGlfwWindow = glfwCreateWindow(mVulkanApplicationProperties.mWindowWidth, mVulkanApplicationProperties.mWindowHeight, "Supernova", nullptr, nullptr);
	if (!mGlfwWindow)
	{
		glfwTerminate();
		throw std::runtime_error("Failed to create a window");
	}

	glfwSetWindowUserPointer(mGlfwWindow, this);
	glfwSetFramebufferSizeCallback(mGlfwWindow, FramebufferResizeCallback);
	glfwSetWindowSizeCallback(mGlfwWindow, WindowResizeCallback);
	glfwSetWindowIconifyCallback(mGlfwWindow, WindowMinimizedCallback);
	glfwSetInputMode(mGlfwWindow, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(mGlfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(mGlfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	int major, minor, revision;
	glfwGetVersion(&major, &minor, &revision);

	std::cout << std::format("GLFW v{}.{}.{}", major, minor, revision) << std::endl;
}

void VulkanRenderer::SetWindowSize(int aWidth, int aHeight)
{
	mVulkanApplicationProperties.mWindowWidth = aWidth;
	mVulkanApplicationProperties.mWindowHeight = aHeight;
}

void VulkanRenderer::FramebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/)
{
	VulkanRenderer* vulkanRenderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
	if (vulkanRenderer->mVulkanApplicationProperties.mIsMinimized)
		return;

	vulkanRenderer->mIsFramebufferResized = true;
}

void VulkanRenderer::WindowResizeCallback(GLFWwindow* window, int width, int height)
{
	VulkanRenderer* vulkanRenderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
	vulkanRenderer->SetWindowSize(width, height);
}

void VulkanRenderer::WindowMinimizedCallback(GLFWwindow* window, int aValue)
{
	VulkanRenderer* vulkanRenderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
	vulkanRenderer->mVulkanApplicationProperties.mIsMinimized = aValue;
	vulkanRenderer->mIsPaused = aValue;
}

VkResult VulkanRenderer::createInstance()
{
	std::vector<const char*> instanceExtensions = {VK_KHR_SURFACE_EXTENSION_NAME};

	std::uint32_t extCount = 0;
	VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr));
	if (extCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extCount);
		if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
		{
			for (VkExtensionProperties& extension : extensions)
			{
				supportedInstanceExtensions.push_back(extension.extensionName);
			}
		}
	}

	if (!enabledInstanceExtensions.empty())
	{
		for (const char* enabledExtension : enabledInstanceExtensions)
		{
			if (std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), enabledExtension) == supportedInstanceExtensions.end())
				std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";

			instanceExtensions.push_back(enabledExtension);
		}
	}

	// Shaders generated by Slang require a certain SPIR-V environment that can't be satisfied by Vulkan 1.0, so we need to expliclity up that to at least 1.1 and enable some required extensions
	if (shaderDir == "Slang")
	{
		if (mVulkanApplicationProperties.mAPIVersion < VK_API_VERSION_1_1)
		{
			mVulkanApplicationProperties.mAPIVersion = VK_API_VERSION_1_1;
		}
		enabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
		enabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = mVulkanApplicationProperties.mApplicationName.c_str();
	appInfo.pEngineName = mVulkanApplicationProperties.mEngineName.c_str();
	appInfo.apiVersion = mVulkanApplicationProperties.mAPIVersion;

	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;

	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
	if (mVulkanApplicationProperties.mIsValidationEnabled)
	{
		vks::debug::setupDebugingMessengerCreateInfo(debugUtilsMessengerCI);
		debugUtilsMessengerCI.pNext = instanceCreateInfo.pNext;
		instanceCreateInfo.pNext = &debugUtilsMessengerCI;
	}

	if (mVulkanApplicationProperties.mIsValidationEnabled || std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedInstanceExtensions.end())
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	const std::vector<const char*> glfwRequiredExtensions = VulkanExampleLocal::GetGlfwRequiredExtensions();
	for (const char* glfwRequiredExtension : glfwRequiredExtensions)
	{
		if (std::ranges::find(instanceExtensions, glfwRequiredExtension) == instanceExtensions.end())
		{
			instanceExtensions.push_back(glfwRequiredExtension);
		}
	}

	if (!instanceExtensions.empty())
	{
		instanceCreateInfo.enabledExtensionCount = (std::uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

#ifndef NDEBUG
		for (const char* instanceExtension : instanceExtensions)
		{
			std::cout << "Enabling instance extension " << instanceExtension << std::endl;
		}
#endif
	}

	const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
	if (mVulkanApplicationProperties.mIsValidationEnabled)
	{
		std::uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
		bool validationLayerPresent = false;
		for (VkLayerProperties& layer : instanceLayerProperties)
		{
			if (strcmp(layer.layerName, validationLayerName) == 0)
			{
				validationLayerPresent = true;
				break;
			}
		}
		if (validationLayerPresent)
		{
			instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
			instanceCreateInfo.enabledLayerCount = 1;
		}
		else
		{
			std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
		}
	}

	// If layer settings are defined, then activate the sample's required layer settings during instance creation.
	// Layer settings are typically used to activate specific features of a layer, such as the Validation Layer's
	// printf feature, or to configure specific capabilities of drivers such as MoltenVK on macOS and/or iOS.
	VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo{VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT};
	if (enabledLayerSettings.size() > 0)
	{
		layerSettingsCreateInfo.settingCount = static_cast<std::uint32_t>(enabledLayerSettings.size());
		layerSettingsCreateInfo.pSettings = enabledLayerSettings.data();
		layerSettingsCreateInfo.pNext = instanceCreateInfo.pNext;
		instanceCreateInfo.pNext = &layerSettingsCreateInfo;
	}

	VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &mVkInstance);
	VK_CHECK_RESULT(glfwCreateWindowSurface(mVkInstance, mGlfwWindow, nullptr, &mVulkanSwapChain.mVkSurfaceKHR));

	// If the debug utils extension is present we set up debug functions, so samples can label objects for debugging
	if (std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedInstanceExtensions.end())
	{
		vks::debugutils::setup(mVkInstance);
	}

	return result;
}

std::string VulkanRenderer::GetWindowTitle(float aDeltaTime) const
{
	return std::format("{} - {} - {:.3f} ms {} fps - {:.3f} ms - {}/{} window - {}/{} framebuffer",
		mVulkanApplicationProperties.mApplicationName,
		vulkanDevice->properties.deviceName,
		(mFrameTime * 1000.f),
		mLastFPS,
		(aDeltaTime * 1000.f),
		mVulkanApplicationProperties.mWindowWidth,
		mVulkanApplicationProperties.mWindowHeight,
		mFramebufferWidth,
		mFramebufferHeight);
}

void VulkanRenderer::destroyCommandBuffers()
{
	vkFreeCommandBuffers(mVkLogicalDevice, mVkCommandPool, static_cast<std::uint32_t>(mVkCommandBuffers.size()), mVkCommandBuffers.data());
}

std::string VulkanRenderer::getShadersPath() const
{
	return getShaderBasePath() + shaderDir + "/";
}

void VulkanRenderer::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(mVkLogicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanRenderer::InitializeSwapchain()
{
	mVulkanSwapChain.InitializeSurface();
}

VkPipelineShaderStageCreateInfo VulkanRenderer::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
	shaderStage.module = vks::tools::loadShader(fileName.c_str(), mVkLogicalDevice);
	shaderStage.pName = "main";
	assert(shaderStage.module != VK_NULL_HANDLE);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

void VulkanRenderer::NextFrame()
{
	const std::chrono::steady_clock::time_point frameTimeStart = std::chrono::high_resolution_clock::now();
	
	PrepareFrame();

	mFrameCounter++;
	const std::chrono::steady_clock::time_point frameTimeEnd = std::chrono::high_resolution_clock::now();
	const float frameTimeDelta = std::chrono::duration<float, std::milli>(frameTimeEnd - frameTimeStart).count();
	mFrameTime = frameTimeDelta / 1000.0f;

	const float fpsTimer = std::chrono::duration<float, std::milli>(frameTimeEnd - mLastTimestamp).count();
	if (fpsTimer > 1000.0f)
	{
		mLastFPS = static_cast<std::uint32_t>(static_cast<float>(mFrameCounter) * (1000.0f / fpsTimer));
		mFrameCounter = 0;
		mLastTimestamp = frameTimeEnd;
	}

	mPreviousEndTime = frameTimeEnd;
}

bool VulkanRenderer::initVulkan()
{
	VkResult result = createInstance();
	if (result != VK_SUCCESS)
	{
		vks::tools::exitFatal("Could not create Vulkan instance : \n" + vks::tools::errorString(result), result);
		return false;
	}

	// If requested, we enable the default validation layers for debugging
	if (mVulkanApplicationProperties.mIsValidationEnabled)
	{
		vks::debug::setupDebugging(mVkInstance);
	}

	// Physical device
	std::uint32_t gpuCount = 0;
	// Get number of available physical devices
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(mVkInstance, &gpuCount, nullptr));
	if (gpuCount == 0)
	{
		vks::tools::exitFatal("No device with Vulkan support found", -1);
		return false;
	}
	// Enumerate devices
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	result = vkEnumeratePhysicalDevices(mVkInstance, &gpuCount, physicalDevices.data());
	if (result != VK_SUCCESS)
	{
		vks::tools::exitFatal("Could not enumerate physical devices : \n" + vks::tools::errorString(result), result);
		return false;
	}

	// GPU selection

	// Select physical device to be used for the Vulkan example
	// Defaults to the first device unless specified by command line
	std::uint32_t selectedDevice = 0;

	physicalDevice = physicalDevices[selectedDevice];

	// Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &mEnabledVkPhysicalDeviceFeatures);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	// Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
	GetEnabledFeatures();

	// Vulkan device creation
	// This is handled by a separate class that gets a logical device representation
	// and encapsulates functions related to a device
	vulkanDevice = new vks::VulkanDevice(physicalDevice);

	result = vulkanDevice->createLogicalDevice(mEnabledVkPhysicalDeviceFeatures, enabledDeviceExtensions, &mVkPhysicalDevice13Features);
	if (result != VK_SUCCESS)
	{
		vks::tools::exitFatal("Could not create Vulkan device: \n" + vks::tools::errorString(result), result);
		return false;
	}
	mVkLogicalDevice = vulkanDevice->logicalDevice;

	// Get a graphics queue from the device
	vkGetDeviceQueue(mVkLogicalDevice, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

	// Find a suitable depth and/or stencil format
	VkBool32 validFormat{false};
	// Samples that make use of stencil will require a depth + stencil format, so we select from a different list
	if (requiresStencil)
	{
		validFormat = vks::tools::getSupportedDepthStencilFormat(physicalDevice, &depthFormat);
	}
	else
	{
		validFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
	}
	assert(validFormat);

	mVulkanSwapChain.setContext(mVkInstance, physicalDevice, mVkLogicalDevice);

	return true;
}

void VulkanRenderer::createCommandPool()
{
	VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {};
	vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	vkCommandPoolCreateInfo.queueFamilyIndex = mVulkanSwapChain.mQueueNodeIndex;
	vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(mVkLogicalDevice, &vkCommandPoolCreateInfo, nullptr, &mVkCommandPool));
}

void VulkanRenderer::OnResizeWindow()
{
	if (!mIsPrepared)
		return;
	
	mIsPrepared = false;
	mIsResized = true;

	// Ensure all operations on the device have been finished before destroying resources
	vkDeviceWaitIdle(mVkLogicalDevice);

	// Recreate swap chain
	SetupSwapchain();

	// Recreate the frame buffers
	vkDestroyImageView(mVkLogicalDevice, depthStencil.mVkImageView, nullptr);
	vkDestroyImage(mVkLogicalDevice, depthStencil.mVkImage, nullptr);
	vkFreeMemory(mVkLogicalDevice, depthStencil.mVkDeviceMemory, nullptr);
	setupDepthStencil();

	for (auto& semaphore : presentCompleteSemaphores)
	{
		vkDestroySemaphore(mVkLogicalDevice, semaphore, nullptr);
	}
	for (auto& semaphore : renderCompleteSemaphores)
	{
		vkDestroySemaphore(mVkLogicalDevice, semaphore, nullptr);
	}
	for (auto& fence : waitFences)
	{
		vkDestroyFence(mVkLogicalDevice, fence, nullptr);
	}
	createSynchronizationPrimitives();

	vkDeviceWaitIdle(mVkLogicalDevice);

	if ((mFramebufferWidth > 0.0f) && (mFramebufferHeight > 0.0f))
	{
		camera.updateAspectRatio(static_cast<float>(mFramebufferWidth) / static_cast<float>(mFramebufferHeight));
	}

	mIsPrepared = true;
}

void VulkanRenderer::handleMouseMove(int32_t x, int32_t y)
{
	int32_t dx = (int32_t)mouseState.position.x - x;
	int32_t dy = (int32_t)mouseState.position.y - y;

	bool handled = false;

	mouseMoved((float)x, (float)y, handled);

	if (handled)
	{
		mouseState.position = glm::vec2((float)x, (float)y);
		return;
	}

	if (mouseState.buttons.left)
	{
		camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
	}
	if (mouseState.buttons.right)
	{
		camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
	}
	if (mouseState.buttons.middle)
	{
		camera.translate(glm::vec3(-dx * 0.005f, -dy * 0.005f, 0.0f));
	}
	mouseState.position = glm::vec2((float)x, (float)y);
}

void VulkanRenderer::SetupSwapchain()
{
	mVulkanSwapChain.CreateSwapchain(mFramebufferWidth, mFramebufferHeight, mVulkanApplicationProperties.mIsVSyncEnabled);
}
