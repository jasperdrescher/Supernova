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
#include <cassert>
#include <string>
#include <cstddef>
#include <array>
#include <format>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <vector>

namespace VulkanExampleLocal
{
	static void GLFWErrorCallback(int aError, const char* aDescription)
	{
		std::cerr << "GLFW error: " << aError << " " << aDescription << std::endl;
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
	: mGLFWWindow(nullptr)
	, mShouldClose(false)
	, mIsFramebufferResized(false)
	, mVkCommandBuffers{VK_NULL_HANDLE}
	, mDefaultClearColor{{0.025f, 0.025f, 0.025f, 1.0f}}
	, mTimer{0.0f}
	, TimerSpeed{0.25f}
	, mIsPaused{false}
	, mIsPrepared{false}
	, mIsResized{false}
	, mFramebufferWidth{0}
	, mFramebufferHeight{0}
	, mFrameTime{1.0f}
	, mVulkanDevice{nullptr}
	, mFrameCounter {0}
	, mLastFPS {0}
	, mVkInstance{VK_NULL_HANDLE}
	, mVkQueue{VK_NULL_HANDLE}
	, mVkDepthFormat{VK_FORMAT_UNDEFINED}
	, mVkCommandPool{VK_NULL_HANDLE}
	, mVkDescriptorPool{VK_NULL_HANDLE}
	, mVkPipelineCache{VK_NULL_HANDLE}
	, mBufferIndexCount{0}
	, mVkPipelineLayout{VK_NULL_HANDLE}
	, mVkPipeline{VK_NULL_HANDLE}
	, mVkDescriptionSetLayout{VK_NULL_HANDLE}
	, mCurrentFrameIndex{0}
	, mVkPhysicalDevice13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES}
	, shaderDir {"GLSL"}
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
	mCamera.type = Camera::CameraType::lookat;
	mCamera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
	mCamera.setRotation(glm::vec3(0.0f));
	mCamera.setPerspective(60.0f, static_cast<float>(mFramebufferWidth) / static_cast<float>(mFramebufferHeight), 1.0f, 256.0f);
}

VulkanRenderer::~VulkanRenderer()
{
	mVulkanSwapChain.CleanUp();

	if (mVulkanDevice->mLogicalVkDevice != VK_NULL_HANDLE)
	{
		if (mVkDescriptorPool != VK_NULL_HANDLE)
			vkDestroyDescriptorPool(mVulkanDevice->mLogicalVkDevice, mVkDescriptorPool, nullptr);

		vkFreeCommandBuffers(mVulkanDevice->mLogicalVkDevice, mVkCommandPool, static_cast<std::uint32_t>(mVkCommandBuffers.size()), mVkCommandBuffers.data());

		for (VkShaderModule& shaderModule : mVkShaderModules)
			vkDestroyShaderModule(mVulkanDevice->mLogicalVkDevice, shaderModule, nullptr);

		vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImageView, nullptr);
		vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImage, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkDeviceMemory, nullptr);

		vkDestroyPipelineCache(mVulkanDevice->mLogicalVkDevice, mVkPipelineCache, nullptr);

		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipeline, nullptr);
		vkDestroyPipelineLayout(mVulkanDevice->mLogicalVkDevice, mVkPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, mVkDescriptionSetLayout, nullptr);
		vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, mVulkanVertexBuffer.mVkBuffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mVulkanVertexBuffer.mVkDeviceMemory, nullptr);
		vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, mVulkanIndexBuffer.mVkBuffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mVulkanIndexBuffer.mVkDeviceMemory, nullptr);
		vkDestroyCommandPool(mVulkanDevice->mLogicalVkDevice, mVkCommandPool, nullptr);

		for (VkSemaphore& semaphore : mVkPresentCompleteSemaphores)
			vkDestroySemaphore(mVulkanDevice->mLogicalVkDevice, semaphore, nullptr);

		for (VkSemaphore& semaphore : mVkRenderCompleteSemaphores)
			vkDestroySemaphore(mVulkanDevice->mLogicalVkDevice, semaphore, nullptr);

		for (std::uint32_t i = 0; i < gMaxConcurrentFrames; i++)
		{
			vkDestroyFence(mVulkanDevice->mLogicalVkDevice, mWaitVkFences[i], nullptr);
			vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, mVulkanUniformBuffers[i].mVkBuffer, nullptr);
			vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mVulkanUniformBuffers[i].mVkDeviceMemory, nullptr);
		}
	}

	if (mVulkanApplicationProperties.mIsValidationEnabled)
		VulkanDebug::DestroyDebugUtilsMessenger(mVkInstance);

	delete mVulkanDevice;

	vkDestroyInstance(mVkInstance, nullptr);
}

void VulkanRenderer::InitializeRenderer()
{
	CreateGlfwWindow();
	InitializeVulkan();
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

	mCamera.update(mFrameTime);

	if (mVulkanDevice->mLogicalVkDevice != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(mVulkanDevice->mLogicalVkDevice);
	}

	glfwSetWindowTitle(mGLFWWindow, GetWindowTitle(aDeltaTime).c_str());
	glfwPollEvents();

	mVulkanApplicationProperties.mIsFocused = glfwGetWindowAttrib(mGLFWWindow, GLFW_FOCUSED);
	mShouldClose = glfwWindowShouldClose(mGLFWWindow);
}

void VulkanRenderer::DestroyRenderer()
{
	glfwDestroyWindow(mGLFWWindow);
	glfwTerminate();
}

std::uint32_t VulkanRenderer::GetMemoryTypeIndex(std::uint32_t typeBits, VkMemoryPropertyFlags properties) const
{
	// Iterate over all memory types available for the device used in this example
	for (std::uint32_t i = 0; i < mVulkanDevice->mVkPhysicalDeviceMemoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((mVulkanDevice->mVkPhysicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		typeBits >>= 1;
	}
	throw std::runtime_error("Could not find a suitable memory type!");
}

void VulkanRenderer::CreateSynchronizationPrimitives()
{
	// Fences are per frame in flight
	for (std::uint32_t i = 0; i < gMaxConcurrentFrames; i++)
	{
		// Fence used to ensure that command buffer has completed exection before using it again
		VkFenceCreateInfo vkFenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		// Create the fences in signaled state (so we don't wait on first render of each command buffer)
		vkFenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		VK_CHECK_RESULT(vkCreateFence(mVulkanDevice->mLogicalVkDevice, &vkFenceCreateInfo, nullptr, &mWaitVkFences[i]));
	}

	// Semaphores are used for correct command ordering within a queue
	// Used to ensure that image presentation is complete before starting to submit again
	mVkPresentCompleteSemaphores.resize(gMaxConcurrentFrames);
	for (VkSemaphore& vkSemaphore : mVkPresentCompleteSemaphores)
	{
		VkSemaphoreCreateInfo vkSemaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalVkDevice, &vkSemaphoreCreateInfo, nullptr, &vkSemaphore));
	}

	// Render completion
	// Semaphore used to ensure that all commands submitted have been finished before submitting the image to the queue
	mVkRenderCompleteSemaphores.resize(mVulkanSwapChain.mVkImages.size());
	for (VkSemaphore& vkSemaphore : mVkRenderCompleteSemaphores)
	{
		VkSemaphoreCreateInfo vkSemaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalVkDevice, &vkSemaphoreCreateInfo, nullptr, &vkSemaphore));
	}
}

// Command buffers are used to record commands to and are submitted to a queue for execution ("rendering")
void VulkanRenderer::CreateCommandBuffers()
{
	// All command buffers are allocated from the same command pool
	VkCommandPoolCreateInfo vkCommandPoolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	vkCommandPoolCreateInfo.queueFamilyIndex = mVulkanSwapChain.mQueueNodeIndex;
	vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(mVulkanDevice->mLogicalVkDevice, &vkCommandPoolCreateInfo, nullptr, &mVkCommandPool));
	// Allocate one command buffer per max. concurrent frame from above pool
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = VulkanInitializers::CommandBufferAllocateInfo(mVkCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, gMaxConcurrentFrames);
	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalVkDevice, &cmdBufAllocateInfo, mVkCommandBuffers.data()));
}

void VulkanRenderer::CreateVertexBuffer()
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
	mBufferIndexCount = static_cast<std::uint32_t>(indices.size());
	std::uint32_t indexBufferSize = mBufferIndexCount * sizeof(std::uint32_t);

	VkMemoryAllocateInfo vkMemoryAllocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	VkMemoryRequirements vkMemoryRequirements;

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
	VulkanBuffer vulkanStagingBuffer;
	VkBufferCreateInfo vkBufferCreateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	vkBufferCreateInfo.size = static_cast<VkDeviceSize>(vertexBufferSize) + indexBufferSize;
	// Buffer is used as the copy source
	vkBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	// Create a host-visible buffer to copy the vertex data to (staging buffer)
	VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalVkDevice, &vkBufferCreateInfo, nullptr, &vulkanStagingBuffer.mVkBuffer));
	vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalVkDevice, vulkanStagingBuffer.mVkBuffer, &vkMemoryRequirements);
	vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
	// Request a host visible memory type that can be used to copy our data to
	// Also request it to be coherent, so that writes are visible to the GPU right after unmapping the buffer
	vkMemoryAllocateInfo.memoryTypeIndex = GetMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &vkMemoryAllocateInfo, nullptr, &vulkanStagingBuffer.mVkDeviceMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalVkDevice, vulkanStagingBuffer.mVkBuffer, vulkanStagingBuffer.mVkDeviceMemory, 0));
	// Map the buffer and copy vertices and indices into it, this way we can use a single buffer as the source for both vertex and index GPU buffers
	uint8_t* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalVkDevice, vulkanStagingBuffer.mVkDeviceMemory, 0, vkMemoryAllocateInfo.allocationSize, 0, (void**)&data));
	memcpy(data, vertices.data(), vertexBufferSize);
	memcpy(((char*)data) + vertexBufferSize, indices.data(), indexBufferSize);

	// Create a device local buffer to which the (host local) vertex data will be copied and which will be used for rendering
	VkBufferCreateInfo vkVertexBufferCreateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	vkVertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vkVertexBufferCreateInfo.size = vertexBufferSize;
	VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalVkDevice, &vkVertexBufferCreateInfo, nullptr, &mVulkanVertexBuffer.mVkBuffer));
	vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalVkDevice, mVulkanVertexBuffer.mVkBuffer, &vkMemoryRequirements);
	vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
	vkMemoryAllocateInfo.memoryTypeIndex = GetMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &vkMemoryAllocateInfo, nullptr, &mVulkanVertexBuffer.mVkDeviceMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalVkDevice, mVulkanVertexBuffer.mVkBuffer, mVulkanVertexBuffer.mVkDeviceMemory, 0));

	// Create a device local buffer to which the (host local) index data will be copied and which will be used for rendering
	VkBufferCreateInfo vkIndexBufferCreateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	vkIndexBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vkIndexBufferCreateInfo.size = indexBufferSize;
	VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalVkDevice, &vkIndexBufferCreateInfo, nullptr, &mVulkanIndexBuffer.mVkBuffer));
	vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalVkDevice, mVulkanIndexBuffer.mVkBuffer, &vkMemoryRequirements);
	vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
	vkMemoryAllocateInfo.memoryTypeIndex = GetMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &vkMemoryAllocateInfo, nullptr, &mVulkanIndexBuffer.mVkDeviceMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalVkDevice, mVulkanIndexBuffer.mVkBuffer, mVulkanIndexBuffer.mVkDeviceMemory, 0));

	// Buffer copies have to be submitted to a queue, so we need a command buffer for them
	VkCommandBuffer vkCopyCommandBuffer;

	VkCommandBufferAllocateInfo vkCommandBufferAllocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	vkCommandBufferAllocateInfo.commandPool = mVkCommandPool;
	vkCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	vkCommandBufferAllocateInfo.commandBufferCount = 1;
	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalVkDevice, &vkCommandBufferAllocateInfo, &vkCopyCommandBuffer));

	VkCommandBufferBeginInfo vkCommandBufferBeginInfo = VulkanInitializers::CommandBufferBeginInfo();
	VK_CHECK_RESULT(vkBeginCommandBuffer(vkCopyCommandBuffer, &vkCommandBufferBeginInfo));
	// Copy vertex and index buffers to the device
	VkBufferCopy vkRegionBufferCopy{};
	vkRegionBufferCopy.size = vertexBufferSize;
	vkCmdCopyBuffer(vkCopyCommandBuffer, vulkanStagingBuffer.mVkBuffer, mVulkanVertexBuffer.mVkBuffer, 1, &vkRegionBufferCopy);
	vkRegionBufferCopy.size = indexBufferSize;
	// Indices are stored after the vertices in the source buffer, so we need to add an offset
	vkRegionBufferCopy.srcOffset = vertexBufferSize;
	vkCmdCopyBuffer(vkCopyCommandBuffer, vulkanStagingBuffer.mVkBuffer, mVulkanIndexBuffer.mVkBuffer, 1, &vkRegionBufferCopy);
	VK_CHECK_RESULT(vkEndCommandBuffer(vkCopyCommandBuffer));

	// Submit the command buffer to the queue to finish the copy
	VkSubmitInfo vkSubmitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	vkSubmitInfo.commandBufferCount = 1;
	vkSubmitInfo.pCommandBuffers = &vkCopyCommandBuffer;

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo vkFenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	VkFence vkFence;
	VK_CHECK_RESULT(vkCreateFence(mVulkanDevice->mLogicalVkDevice, &vkFenceCreateInfo, nullptr, &vkFence));
	// Submit copies to the queue
	VK_CHECK_RESULT(vkQueueSubmit(mVkQueue, 1, &vkSubmitInfo, vkFence));
	// Wait for the fence to signal that command buffer has finished executing
	VK_CHECK_RESULT(vkWaitForFences(mVulkanDevice->mLogicalVkDevice, 1, &vkFence, VK_TRUE, gDefaultFenceTimeoutNS));
	vkDestroyFence(mVulkanDevice->mLogicalVkDevice, vkFence, nullptr);
	vkFreeCommandBuffers(mVulkanDevice->mLogicalVkDevice, mVkCommandPool, 1, &vkCopyCommandBuffer);

	// The fence made sure copies are finished, so we can safely delete the staging buffer
	vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, vulkanStagingBuffer.mVkBuffer, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, vulkanStagingBuffer.mVkDeviceMemory, nullptr);
}

void VulkanRenderer::CreateDescriptors()
{
	// Descriptors are allocated from a pool, that tells the implementation how many and what types of descriptors we are going to use (at maximum)
	VkDescriptorPoolSize vkDescriptorPoolCounts[1]{};
	// This example only one descriptor type (uniform buffer)
	vkDescriptorPoolCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	// We have one buffer (and as such descriptor) per frame
	vkDescriptorPoolCounts[0].descriptorCount = gMaxConcurrentFrames;
	// For additional types you need to add new entries in the type count list
	// E.g. for two combined image samplers :
	// typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	// typeCounts[1].descriptorCount = 2;

	// Create the global descriptor pool
	// All descriptors used in this example are allocated from this pool
	VkDescriptorPoolCreateInfo vkDescriptorPoolCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	vkDescriptorPoolCreateInfo.poolSizeCount = 1;
	vkDescriptorPoolCreateInfo.pPoolSizes = vkDescriptorPoolCounts;
	// Set the max. number of descriptor sets that can be requested from this pool (requesting beyond this limit will result in an error)
	// Our sample will create one set per uniform buffer per frame
	vkDescriptorPoolCreateInfo.maxSets = gMaxConcurrentFrames;
	VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalVkDevice, &vkDescriptorPoolCreateInfo, nullptr, &mVkDescriptorPool));

	// Descriptor set layouts define the interface between our application and the shader
	// Basically connects the different shader stages to descriptors for binding uniform buffers, image samplers, etc.
	// So every shader binding should map to one descriptor set layout binding
	// Binding 0: Uniform buffer (Vertex shader)
	VkDescriptorSetLayoutBinding vkDescriptorSetLayoutBinding{};
	vkDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vkDescriptorSetLayoutBinding.descriptorCount = 1;
	vkDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo vkDescriptorSetLayoutCreateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	vkDescriptorSetLayoutCreateInfo.bindingCount = 1;
	vkDescriptorSetLayoutCreateInfo.pBindings = &vkDescriptorSetLayoutBinding;
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, &vkDescriptorSetLayoutCreateInfo, nullptr, &mVkDescriptionSetLayout));

	// Where the descriptor set layout is the interface, the descriptor set points to actual data
	// Descriptors that are changed per frame need to be multiplied, so we can update descriptor n+1 while n is still used by the GPU, so we create one per max frame in flight
	for (std::uint32_t i = 0; i < gMaxConcurrentFrames; i++)
	{
		VkDescriptorSetAllocateInfo vkDescriptorSetAllocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		vkDescriptorSetAllocateInfo.descriptorPool = mVkDescriptorPool;
		vkDescriptorSetAllocateInfo.descriptorSetCount = 1;
		vkDescriptorSetAllocateInfo.pSetLayouts = &mVkDescriptionSetLayout;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &vkDescriptorSetAllocateInfo, &mVulkanUniformBuffers[i].mVkDescriptorSet));

		// Update the descriptor set determining the shader binding points
		// For every binding point used in a shader there needs to be one
		// descriptor set matching that binding point
		VkWriteDescriptorSet vkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

		// The buffer's information is passed using a descriptor info structure
		VkDescriptorBufferInfo vkDescriptorBufferInfo{};
		vkDescriptorBufferInfo.buffer = mVulkanUniformBuffers[i].mVkBuffer;
		vkDescriptorBufferInfo.range = sizeof(VulkanShaderData);

		// Binding 0 : Uniform buffer
		vkWriteDescriptorSet.dstSet = mVulkanUniformBuffers[i].mVkDescriptorSet;
		vkWriteDescriptorSet.descriptorCount = 1;
		vkWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vkWriteDescriptorSet.pBufferInfo = &vkDescriptorBufferInfo;
		vkWriteDescriptorSet.dstBinding = 0;
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, 1, &vkWriteDescriptorSet, 0, nullptr);
	}
}

void VulkanRenderer::SetupDepthStencil()
{
	// Create an optimal tiled image used as the depth stencil attachment
	VkImageCreateInfo vkImageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	vkImageCreateInfo.format = mVkDepthFormat;
	vkImageCreateInfo.extent = {mFramebufferWidth, mFramebufferHeight, 1};
	vkImageCreateInfo.mipLevels = 1;
	vkImageCreateInfo.arrayLayers = 1;
	vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	vkImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	vkImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalVkDevice, &vkImageCreateInfo, nullptr, &mVulkanDepthStencil.mVkImage));

	// Allocate memory for the image (device local) and bind it to our image
	VkMemoryAllocateInfo vkMemoryAllocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	VkMemoryRequirements vkMemoryRequirements;
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImage, &vkMemoryRequirements);
	vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
	vkMemoryAllocateInfo.memoryTypeIndex = GetMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &vkMemoryAllocateInfo, nullptr, &mVulkanDepthStencil.mVkDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImage, mVulkanDepthStencil.mVkDeviceMemory, 0));

	// Create a view for the depth stencil image
	// Images aren't directly accessed in Vulkan, but rather through views described by a subresource range
	// This allows for multiple views of one image with differing ranges (e.g. for different layers)
	VkImageViewCreateInfo vkImageViewCreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	vkImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vkImageViewCreateInfo.format = mVkDepthFormat;
	vkImageViewCreateInfo.subresourceRange = {};
	vkImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT)
	if (mVkDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
	{
		vkImageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	vkImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	vkImageViewCreateInfo.subresourceRange.levelCount = 1;
	vkImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	vkImageViewCreateInfo.subresourceRange.layerCount = 1;
	vkImageViewCreateInfo.image = mVulkanDepthStencil.mVkImage;
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalVkDevice, &vkImageViewCreateInfo, nullptr, &mVulkanDepthStencil.mVkImageView));
}

VkShaderModule VulkanRenderer::LoadSPIRVShader(const std::string& filename) const
{
	size_t shaderSize{0};
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
		VK_CHECK_RESULT(vkCreateShaderModule(mVulkanDevice->mLogicalVkDevice, &shaderModuleCI, nullptr, &shaderModule));

		delete[] shaderCode;

		return shaderModule;
	}
	else
	{
		std::cerr << "Error: Could not open shader file \"" << filename << "\"" << std::endl;
		return VK_NULL_HANDLE;
	}
}

void VulkanRenderer::CreatePipeline()
{
	// The pipeline layout is the interface telling the pipeline what type of descriptors will later be bound
	VkPipelineLayoutCreateInfo vkPipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	vkPipelineLayoutCreateInfo.setLayoutCount = 1;
	vkPipelineLayoutCreateInfo.pSetLayouts = &mVkDescriptionSetLayout;
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalVkDevice, &vkPipelineLayoutCreateInfo, nullptr, &mVkPipelineLayout));

	// Create the graphics pipeline used in this example
	// Vulkan uses the concept of rendering pipelines to encapsulate fixed states, replacing OpenGL's complex state machine
	// A pipeline is then stored and hashed on the GPU making pipeline changes very fast

	VkGraphicsPipelineCreateInfo vkGraphicsPipelineCreateInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	// The layout used for this pipeline (can be shared among multiple pipelines using the same layout)
	vkGraphicsPipelineCreateInfo.layout = mVkPipelineLayout;

	// Construct the different states making up the pipeline

	// Input assembly state describes how primitives are assembled
	// This pipeline will assemble vertex data as a triangle lists (though we only use one triangle)
	VkPipelineInputAssemblyStateCreateInfo vkPipelineInputAssemblyStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	vkPipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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
	VkPipelineColorBlendAttachmentState vkPipelineColorBlendAttachmentState{};
	vkPipelineColorBlendAttachmentState.colorWriteMask = 0xf;
	vkPipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo vkPipelineColorBlendStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	vkPipelineColorBlendStateCreateInfo.attachmentCount = 1;
	vkPipelineColorBlendStateCreateInfo.pAttachments = &vkPipelineColorBlendAttachmentState;

	// Viewport state sets the number of viewports and scissor used in this pipeline
	// Note: This is actually overridden by the dynamic states (see below)
	VkPipelineViewportStateCreateInfo vkPipelineViewportStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	vkPipelineViewportStateCreateInfo.viewportCount = 1;
	vkPipelineViewportStateCreateInfo.scissorCount = 1;

	// Enable dynamic states
	// Most states are baked into the pipeline, but there is somee state that can be dynamically changed within the command buffer to mak e things easuer
	// To be able to change these we need do specify which dynamic states will be changed using this pipeline. Their actual states are set later on in the command buffer
	std::vector<VkDynamicState> vkDynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo vkPipelineDynamicStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	vkPipelineDynamicStateCreateInfo.pDynamicStates = vkDynamicStateEnables.data();
	vkPipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<std::uint32_t>(vkDynamicStateEnables.size());

	// Depth and stencil state containing depth and stencil compare and test operations
	// We only use depth tests and want depth tests and writes to be enabled and compare with less or equal
	VkPipelineDepthStencilStateCreateInfo vkPipelineDepthStencilStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	vkPipelineDepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	vkPipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	vkPipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	vkPipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	vkPipelineDepthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
	vkPipelineDepthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
	vkPipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
	vkPipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	vkPipelineDepthStencilStateCreateInfo.front = vkPipelineDepthStencilStateCreateInfo.back;

	// This example does not make use of multi sampling (for anti-aliasing), the state must still be set and passed to the pipeline
	VkPipelineMultisampleStateCreateInfo vkPipelineMultisampleStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	vkPipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Vertex input descriptions
	// Specifies the vertex input parameters for a pipeline

	// Vertex input binding
	// This example uses a single vertex input binding at binding point 0 (see vkCmdBindVertexBuffers)
	VkVertexInputBindingDescription vkVertexInputBindingDescription{};
	vkVertexInputBindingDescription.binding = 0;
	vkVertexInputBindingDescription.stride = sizeof(VulkanVertex);
	vkVertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// Input attribute bindings describe shader attribute locations and memory layouts
	std::array<VkVertexInputAttributeDescription, 2> vkVertexInputAttributeDescriptions{};
	// These match the following shader layout (see triangle.vert):
	//	layout (location = 0) in vec3 inPos;
	//	layout (location = 1) in vec3 inColor;
	// Attribute location 0: Position
	vkVertexInputAttributeDescriptions[0].binding = 0;
	vkVertexInputAttributeDescriptions[0].location = 0;
	// Position attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
	vkVertexInputAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vkVertexInputAttributeDescriptions[0].offset = offsetof(VulkanVertex, mVertexPosition);
	// Attribute location 1: Color
	vkVertexInputAttributeDescriptions[1].binding = 0;
	vkVertexInputAttributeDescriptions[1].location = 1;
	// Color attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
	vkVertexInputAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vkVertexInputAttributeDescriptions[1].offset = offsetof(VulkanVertex, mVertexColor);

	// Vertex input state used for pipeline creation
	VkPipelineVertexInputStateCreateInfo vkPipelineVertexInputStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
	vkPipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vkPipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &vkVertexInputBindingDescription;
	vkPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 2;
	vkPipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vkVertexInputAttributeDescriptions.data();

	// Shaders
	std::array<VkPipelineShaderStageCreateInfo, 2> vkPipelineShaderStages{};

	// Vertex shader
	vkPipelineShaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vkPipelineShaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	vkPipelineShaderStages[0].module = LoadSPIRVShader(getShadersPath() + "triangle/triangle.vert.spv");
	vkPipelineShaderStages[0].pName = "main";
	assert(vkPipelineShaderStages[0].module != VK_NULL_HANDLE);

	// Fragment shader
	vkPipelineShaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vkPipelineShaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	vkPipelineShaderStages[1].module = LoadSPIRVShader(getShadersPath() + "triangle/triangle.frag.spv");
	vkPipelineShaderStages[1].pName = "main";
	assert(vkPipelineShaderStages[1].module != VK_NULL_HANDLE);

	// Set pipeline shader stage info
	vkGraphicsPipelineCreateInfo.stageCount = static_cast<std::uint32_t>(vkPipelineShaderStages.size());
	vkGraphicsPipelineCreateInfo.pStages = vkPipelineShaderStages.data();

	// Attachment information for dynamic rendering
	VkPipelineRenderingCreateInfoKHR vkPipelineRenderingCreateInfoKHR{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
	vkPipelineRenderingCreateInfoKHR.colorAttachmentCount = 1;
	vkPipelineRenderingCreateInfoKHR.pColorAttachmentFormats = &mVulkanSwapChain.mColorVkFormat;
	vkPipelineRenderingCreateInfoKHR.depthAttachmentFormat = mVkDepthFormat;
	vkPipelineRenderingCreateInfoKHR.stencilAttachmentFormat = mVkDepthFormat;

	// Assign the pipeline states to the pipeline creation info structure
	vkGraphicsPipelineCreateInfo.pVertexInputState = &vkPipelineVertexInputStateCreateInfo;
	vkGraphicsPipelineCreateInfo.pInputAssemblyState = &vkPipelineInputAssemblyStateCreateInfo;
	vkGraphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCI;
	vkGraphicsPipelineCreateInfo.pColorBlendState = &vkPipelineColorBlendStateCreateInfo;
	vkGraphicsPipelineCreateInfo.pMultisampleState = &vkPipelineMultisampleStateCreateInfo;
	vkGraphicsPipelineCreateInfo.pViewportState = &vkPipelineViewportStateCreateInfo;
	vkGraphicsPipelineCreateInfo.pDepthStencilState = &vkPipelineDepthStencilStateCreateInfo;
	vkGraphicsPipelineCreateInfo.pDynamicState = &vkPipelineDynamicStateCreateInfo;
	vkGraphicsPipelineCreateInfo.pNext = &vkPipelineRenderingCreateInfoKHR;

	// Create rendering pipeline using the specified states
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mVkPipelineCache, 1, &vkGraphicsPipelineCreateInfo, nullptr, &mVkPipeline));

	// Shader modules can safely be destroyed when the pipeline has been created
	vkDestroyShaderModule(mVulkanDevice->mLogicalVkDevice, vkPipelineShaderStages[0].module, nullptr);
	vkDestroyShaderModule(mVulkanDevice->mLogicalVkDevice, vkPipelineShaderStages[1].module, nullptr);
}

void VulkanRenderer::CreateUniformBuffers()
{
	// Prepare and initialize the per-frame uniform buffer blocks containing shader uniforms
	// Single uniforms like in OpenGL are no longer present in Vulkan. All shader uniforms are passed via uniform buffer blocks
	VkBufferCreateInfo vkBufferCreateInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	vkBufferCreateInfo.size = sizeof(VulkanShaderData);
	// This buffer will be used as a uniform buffer
	vkBufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	// Create the buffers
	for (std::uint32_t i = 0; i < gMaxConcurrentFrames; i++)
	{
		VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalVkDevice, &vkBufferCreateInfo, nullptr, &mVulkanUniformBuffers[i].mVkBuffer));
		// Get memory requirements including size, alignment and memory type based on the buffer type we request (uniform buffer)
		VkMemoryRequirements vkMemoryRequirements;
		vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalVkDevice, mVulkanUniformBuffers[i].mVkBuffer, &vkMemoryRequirements);
		VkMemoryAllocateInfo vkMemoryAllocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		// Note that we use the size we got from the memory requirements and not the actual buffer size, as the former may be larger due to alignment requirements of the device
		vkMemoryAllocateInfo.allocationSize = vkMemoryRequirements.size;
		// Get the memory type index that supports host visible memory access
		// Most implementations offer multiple memory types and selecting the correct one to allocate memory from is crucial
		// We also want the buffer to be host coherent so we don't have to flush (or sync after every update).
		vkMemoryAllocateInfo.memoryTypeIndex = GetMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		// Allocate memory for the uniform buffer
		VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &vkMemoryAllocateInfo, nullptr, &(mVulkanUniformBuffers[i].mVkDeviceMemory)));
		// Bind memory to buffer
		VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalVkDevice, mVulkanUniformBuffers[i].mVkBuffer, mVulkanUniformBuffers[i].mVkDeviceMemory, 0));
		// We map the buffer once, so we can update it without having to map it again
		VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalVkDevice, mVulkanUniformBuffers[i].mVkDeviceMemory, 0, sizeof(VulkanShaderData), 0, (void**)&mVulkanUniformBuffers[i].mMappedData));
	}

}

void VulkanRenderer::PrepareVulkanResources()
{
	InitializeSwapchain();
	CreateCommandPool();
	SetupSwapchain();
	SetupDepthStencil();
	CreatePipelineCache();
	CreateSynchronizationPrimitives();
	CreateCommandBuffers();
	CreateVertexBuffer();
	CreateUniformBuffers();
	CreateDescriptors();
	CreatePipeline();

	mIsPrepared = true;
}

void VulkanRenderer::PrepareFrame()
{
	// Use a fence to wait until the command buffer has finished execution before using it again
	vkWaitForFences(mVulkanDevice->mLogicalVkDevice, 1, &mWaitVkFences[mCurrentFrameIndex], VK_TRUE, UINT64_MAX);
	VK_CHECK_RESULT(vkResetFences(mVulkanDevice->mLogicalVkDevice, 1, &mWaitVkFences[mCurrentFrameIndex]));

	// Get the next swap chain image from the implementation
	// Note that the implementation is free to return the images in any order, so we must use the acquire function and can't just cycle through the images/imageIndex on our own
	std::uint32_t imageIndex{0};
	VkResult result = vkAcquireNextImageKHR(mVulkanDevice->mLogicalVkDevice, mVulkanSwapChain.mVkSwapchainKHR, UINT64_MAX, mVkPresentCompleteSemaphores[mCurrentFrameIndex], VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		OnResizeWindow();
		return;
	}
	else if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR))
	{
		throw std::runtime_error("Could not acquire the next swap chain image!");
	}

	// Update the uniform buffer for the next frame
	VulkanShaderData shaderData{};
	shaderData.projectionMatrix = mCamera.matrices.perspective;
	shaderData.viewMatrix = mCamera.matrices.view;
	shaderData.modelMatrix = glm::mat4(1.0f);
	// Copy the current matrices to the current frame's uniform buffer. As we requested a host coherent memory type for the uniform buffer, the write is instantly visible to the GPU.
	memcpy(mVulkanUniformBuffers[mCurrentFrameIndex].mMappedData, &shaderData, sizeof(VulkanShaderData));

	// Build the command buffer for the next frame to render
	vkResetCommandBuffer(mVkCommandBuffers[mCurrentFrameIndex], 0);
	VkCommandBufferBeginInfo cmdBufInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	const VkCommandBuffer commandBuffer = mVkCommandBuffers[mCurrentFrameIndex];
	VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

	// With dynamic rendering we need to explicitly add layout transitions by using barriers, this set of barriers prepares the color and depth images for output
	VulkanTools::InsertImageMemoryBarrier(commandBuffer, mVulkanSwapChain.mVkImages[imageIndex], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
	VulkanTools::InsertImageMemoryBarrier(commandBuffer, mVulkanDepthStencil.mVkImage, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1});

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
	depthStencilAttachment.imageView = mVulkanDepthStencil.mVkImageView;
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
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelineLayout, 0, 1, &mVulkanUniformBuffers[mCurrentFrameIndex].mVkDescriptorSet, 0, nullptr);
	// The pipeline (state object) contains all states of the rendering pipeline, binding it will set all the states specified at pipeline creation time
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipeline);
	// Bind triangle vertex buffer (contains position and colors)
	VkDeviceSize offsets[1]{0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mVulkanVertexBuffer.mVkBuffer, offsets);
	// Bind triangle index buffer
	vkCmdBindIndexBuffer(commandBuffer, mVulkanIndexBuffer.mVkBuffer, 0, VK_INDEX_TYPE_UINT32);
	// Draw indexed triangle
	vkCmdDrawIndexed(commandBuffer, mBufferIndexCount, 1, 0, 0, 0);
	// Finish the current dynamic rendering section
	vkCmdEndRendering(commandBuffer);

	// This barrier prepares the color image for presentation, we don't need to care for the depth image
	VulkanTools::InsertImageMemoryBarrier(commandBuffer, mVulkanSwapChain.mVkImages[imageIndex], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_NONE, VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
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
	submitInfo.pWaitSemaphores = &mVkPresentCompleteSemaphores[mCurrentFrameIndex];
	submitInfo.waitSemaphoreCount = 1;
	// Semaphore to be signaled when command buffers have completed
	submitInfo.pSignalSemaphores = &mVkRenderCompleteSemaphores[imageIndex];
	submitInfo.signalSemaphoreCount = 1;

	// Submit to the graphics queue passing a wait fence
	VK_CHECK_RESULT(vkQueueSubmit(mVkQueue, 1, &submitInfo, mWaitVkFences[mCurrentFrameIndex]));

	// Present the current frame buffer to the swap chain
	// Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
	// This ensures that the image is not presented to the windowing system until all commands have been submitted
	VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &mVkRenderCompleteSemaphores[imageIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &mVulkanSwapChain.mVkSwapchainKHR;
	presentInfo.pImageIndices = &imageIndex;
	result = vkQueuePresentKHR(mVkQueue, &presentInfo);
	if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR) || mIsFramebufferResized)
	{
		mIsFramebufferResized = false;

		if (!mVulkanApplicationProperties.mIsMinimized)
			OnResizeWindow();
	}
	else if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Could not present the image to the swap chain!");
	}

	// Select the next frame to render to, based on the max. no. of concurrent frames
	mCurrentFrameIndex = (mCurrentFrameIndex + 1) % gMaxConcurrentFrames;
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
	mGLFWWindow = glfwCreateWindow(mVulkanApplicationProperties.mWindowWidth, mVulkanApplicationProperties.mWindowHeight, "Supernova", nullptr, nullptr);
	if (!mGLFWWindow)
	{
		glfwTerminate();
		throw std::runtime_error("Failed to create a window");
	}

	glfwSetWindowUserPointer(mGLFWWindow, this);
	glfwSetKeyCallback(mGLFWWindow, KeyCallback);
	glfwSetFramebufferSizeCallback(mGLFWWindow, FramebufferResizeCallback);
	glfwSetWindowSizeCallback(mGLFWWindow, WindowResizeCallback);
	glfwSetWindowIconifyCallback(mGLFWWindow, WindowMinimizedCallback);
	glfwSetInputMode(mGLFWWindow, GLFW_STICKY_KEYS, GLFW_TRUE);
	glfwSetInputMode(mGLFWWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(mGLFWWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	int major, minor, revision;
	glfwGetVersion(&major, &minor, &revision);

	std::cout << std::format("GLFW v{}.{}.{}", major, minor, revision) << std::endl;
}

void VulkanRenderer::SetWindowSize(int aWidth, int aHeight)
{
	mVulkanApplicationProperties.mWindowWidth = aWidth;
	mVulkanApplicationProperties.mWindowHeight = aHeight;
}

void VulkanRenderer::KeyCallback(GLFWwindow* aWindow, int aKey, int /*aScancode*/, int aAction, int /*aMode*/)
{
	VulkanRenderer* vulkanRenderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(aWindow));
	if (aKey == GLFW_KEY_ESCAPE && aAction != GLFW_RELEASE)
	{
		glfwSetWindowShouldClose(vulkanRenderer->mGLFWWindow, GLFW_TRUE);
	}
}

void VulkanRenderer::FramebufferResizeCallback(GLFWwindow* aWindow, int /*aWidth*/, int /*aHeight*/)
{
	VulkanRenderer* vulkanRenderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(aWindow));
	if (vulkanRenderer->mVulkanApplicationProperties.mIsMinimized)
		return;

	vulkanRenderer->mIsFramebufferResized = true;
}

void VulkanRenderer::WindowResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight)
{
	VulkanRenderer* vulkanRenderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(aWindow));
	vulkanRenderer->SetWindowSize(aWidth, aHeight);
}

void VulkanRenderer::WindowMinimizedCallback(GLFWwindow* aWindow, int aValue)
{
	VulkanRenderer* vulkanRenderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(aWindow));
	vulkanRenderer->mVulkanApplicationProperties.mIsMinimized = aValue;
	vulkanRenderer->mIsPaused = aValue;
}

VkResult VulkanRenderer::CreateVkInstance()
{
	std::vector<const char*> instanceExtensions = {VK_KHR_SURFACE_EXTENSION_NAME};

	std::uint32_t extensionCount = 0;
	VK_CHECK_RESULT(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));
	if (extensionCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extensionCount);
		if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, &extensions.front()) == VK_SUCCESS)
		{
			for (const VkExtensionProperties& extension : extensions)
			{
				mSupportedInstanceExtensions.push_back(extension.extensionName);
			}
		}
	}

	if (!mEnabledInstanceExtensions.empty())
	{
		for (const char* enabledExtension : mEnabledInstanceExtensions)
		{
			if (std::find(mSupportedInstanceExtensions.begin(), mSupportedInstanceExtensions.end(), enabledExtension) == mSupportedInstanceExtensions.end())
				std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level" << std::endl;

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
		mEnabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
		mEnabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
	}

	VkApplicationInfo vkApplicationInfo{};
	vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vkApplicationInfo.pApplicationName = mVulkanApplicationProperties.mApplicationName.c_str();
	vkApplicationInfo.pEngineName = mVulkanApplicationProperties.mEngineName.c_str();
	vkApplicationInfo.apiVersion = mVulkanApplicationProperties.mAPIVersion;

	VkInstanceCreateInfo vkInstanceCreateInfo{};
	vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;

	VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfo{};
	if (mVulkanApplicationProperties.mIsValidationEnabled)
	{
		VulkanDebug::SetupDebugingMessengerCreateInfo(vkDebugUtilsMessengerCreateInfo);
		vkDebugUtilsMessengerCreateInfo.pNext = vkInstanceCreateInfo.pNext;
		vkInstanceCreateInfo.pNext = &vkDebugUtilsMessengerCreateInfo;
	}

	if (mVulkanApplicationProperties.mIsValidationEnabled || std::find(mSupportedInstanceExtensions.begin(), mSupportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != mSupportedInstanceExtensions.end())
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
		vkInstanceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(instanceExtensions.size());
		vkInstanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

#ifndef NDEBUG
		for (const char* instanceExtension : instanceExtensions)
		{
			std::cout << "Enabling instance extension " << instanceExtension << std::endl;
		}
#endif
	}

	if (mVulkanApplicationProperties.mIsValidationEnabled)
	{
		const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
		std::uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
		bool isValidationLayerPresent = false;
		for (VkLayerProperties& layer : instanceLayerProperties)
		{
			if (std::strcmp(layer.layerName, validationLayerName) == 0)
			{
				isValidationLayerPresent = true;
				break;
			}
		}
		if (isValidationLayerPresent)
		{
			vkInstanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
			vkInstanceCreateInfo.enabledLayerCount = 1;
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
	if (mEnabledLayerSettings.size() > 0)
	{
		layerSettingsCreateInfo.settingCount = static_cast<std::uint32_t>(mEnabledLayerSettings.size());
		layerSettingsCreateInfo.pSettings = mEnabledLayerSettings.data();
		layerSettingsCreateInfo.pNext = vkInstanceCreateInfo.pNext;
		vkInstanceCreateInfo.pNext = &layerSettingsCreateInfo;
	}

	VkResult result = vkCreateInstance(&vkInstanceCreateInfo, nullptr, &mVkInstance);
	VK_CHECK_RESULT(glfwCreateWindowSurface(mVkInstance, mGLFWWindow, nullptr, &mVulkanSwapChain.mVkSurfaceKHR));

	// If the debug utils extension is present we set up debug functions, so samples can label objects for debugging
	if (std::find(mSupportedInstanceExtensions.begin(), mSupportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != mSupportedInstanceExtensions.end())
	{
		VulkanDebug::SetupDebugUtils(mVkInstance);
	}

	return result;
}

void VulkanRenderer::CreateVulkanDevice()
{
	std::uint32_t physicalDeviceCount = 0;
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(mVkInstance, &physicalDeviceCount, nullptr));
	if (physicalDeviceCount == 0)
	{
		throw std::runtime_error(std::format("No device with Vulkan support found: {}", VulkanTools::GetErrorString(VK_ERROR_DEVICE_LOST)));
	}

	std::vector<VkPhysicalDevice> vkPhysicalDevices(physicalDeviceCount);
	VkResult result = vkEnumeratePhysicalDevices(mVkInstance, &physicalDeviceCount, vkPhysicalDevices.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error(std::format("Could not enumerate physical devices: {}", VulkanTools::GetErrorString(result)));
	}

	std::uint32_t selectedDevice = 0;
	VkPhysicalDevice vkPhysicalDevice = vkPhysicalDevices[selectedDevice];
	mVulkanDevice = new VulkanDevice();
	mVulkanDevice->CreatePhysicalDevice(vkPhysicalDevice);
	
	result = mVulkanDevice->CreateLogicalDevice(mVulkanDevice->mEnabledVkPhysicalDeviceFeatures, mEnabledDeviceExtensions, &mVkPhysicalDevice13Features);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error(std::format("Could not create Vulkan device: {}", VulkanTools::GetErrorString(result)));
	}

	mVulkanDevice->mLogicalVkDevice = mVulkanDevice->mLogicalVkDevice;
}

std::string VulkanRenderer::GetWindowTitle(float aDeltaTime) const
{
	return std::format("{} - {} - {:.3f} ms {} fps - {:.3f} ms - {}/{} window - {}/{} framebuffer",
		mVulkanApplicationProperties.mApplicationName,
		mVulkanDevice->mVkPhysicalDeviceProperties.deviceName,
		(mFrameTime * 1000.f),
		mLastFPS,
		(aDeltaTime * 1000.f),
		mVulkanApplicationProperties.mWindowWidth,
		mVulkanApplicationProperties.mWindowHeight,
		mFramebufferWidth,
		mFramebufferHeight);
}

std::string VulkanRenderer::getShadersPath() const
{
	return GetShaderBasePath() + shaderDir + "/";
}

void VulkanRenderer::CreatePipelineCache()
{
	VkPipelineCacheCreateInfo vkPipelineCacheCreateInfo = {};
	vkPipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(mVulkanDevice->mLogicalVkDevice, &vkPipelineCacheCreateInfo, nullptr, &mVkPipelineCache));
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
	shaderStage.module = VulkanTools::LoadShader(fileName.c_str(), mVulkanDevice->mLogicalVkDevice);
	shaderStage.pName = "main";
	assert(shaderStage.module != VK_NULL_HANDLE);
	mVkShaderModules.push_back(shaderStage.module);
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

void VulkanRenderer::InitializeVulkan()
{
	VkResult result = CreateVkInstance();
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error(std::format("Could not create Vulkan instance: {}", VulkanTools::GetErrorString(result)));
	}

	// If requested, we enable the default validation layers for debugging
	if (mVulkanApplicationProperties.mIsValidationEnabled)
	{
		VulkanDebug::SetupDebugUtilsMessenger(mVkInstance);
	}

	CreateVulkanDevice();

	// Get a graphics queue from the device
	vkGetDeviceQueue(mVulkanDevice->mLogicalVkDevice, mVulkanDevice->mQueueFamilyIndices.graphics, 0, &mVkQueue);

	// Find a suitable depth and/or stencil format
	VkBool32 validFormat{false};

	// Applications that make use of stencil will require a depth + stencil format
	validFormat = VulkanTools::GetSupportedDepthFormat(mVulkanDevice->mVkPhysicalDevice, &mVkDepthFormat);
	assert(validFormat);

	mVulkanSwapChain.SetContext(mVkInstance, mVulkanDevice);
}

void VulkanRenderer::CreateCommandPool()
{
	VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {};
	vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	vkCommandPoolCreateInfo.queueFamilyIndex = mVulkanSwapChain.mQueueNodeIndex;
	vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(mVulkanDevice->mLogicalVkDevice, &vkCommandPoolCreateInfo, nullptr, &mVkCommandPool));
}

void VulkanRenderer::OnResizeWindow()
{
	if (!mIsPrepared)
		return;
	
	mIsPrepared = false;
	mIsResized = true;

	// Ensure all operations on the device have been finished before destroying resources
	vkDeviceWaitIdle(mVulkanDevice->mLogicalVkDevice);

	// Recreate swap chain
	SetupSwapchain();

	// Recreate the frame buffers
	vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImageView, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImage, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkDeviceMemory, nullptr);
	SetupDepthStencil();

	for (VkSemaphore& vkPresentCompleteSemaphore : mVkPresentCompleteSemaphores)
		vkDestroySemaphore(mVulkanDevice->mLogicalVkDevice, vkPresentCompleteSemaphore, nullptr);
	
	for (VkSemaphore& vkRendercompleteSemaphore : mVkRenderCompleteSemaphores)
		vkDestroySemaphore(mVulkanDevice->mLogicalVkDevice, vkRendercompleteSemaphore, nullptr);
	
	for (VkFence& waitVkFence : mWaitVkFences)
		vkDestroyFence(mVulkanDevice->mLogicalVkDevice, waitVkFence, nullptr);
	
	CreateSynchronizationPrimitives();

	vkDeviceWaitIdle(mVulkanDevice->mLogicalVkDevice);

	if ((mFramebufferWidth > 0.0f) && (mFramebufferHeight > 0.0f))
	{
		mCamera.updateAspectRatio(static_cast<float>(mFramebufferWidth) / static_cast<float>(mFramebufferHeight));
	}

	mIsPrepared = true;
}

void VulkanRenderer::handleMouseMove(std::int32_t x, std::int32_t y)
{
	std::int32_t dx = static_cast<std::int32_t>(mMouseState.mPosition.x - x);
	std::int32_t dy = static_cast<std::uint32_t>(mMouseState.mPosition.y - y);

	bool handled = false;

	if (handled)
	{
		mMouseState.mPosition = glm::vec2(static_cast<float>(x), static_cast<float>(y));
		return;
	}

	if (mMouseState.mButtons.mIsLeftDown)
	{
		mCamera.rotate(glm::vec3(dy * mCamera.mRotationSpeed, -dx * mCamera.mRotationSpeed, 0.0f));
	}
	if (mMouseState.mButtons.mIsRightDown)
	{
		mCamera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f));
	}
	if (mMouseState.mButtons.mIsMiddleDown)
	{
		mCamera.translate(glm::vec3(-dx * 0.005f, -dy * 0.005f, 0.0f));
	}
	mMouseState.mPosition = glm::vec2(static_cast<float>(x), static_cast<float>(y));
}

void VulkanRenderer::SetupSwapchain()
{
	mVulkanSwapChain.CreateSwapchain(mFramebufferWidth, mFramebufferHeight, mVulkanApplicationProperties.mIsVSyncEnabled);
}
