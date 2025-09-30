#include "VulkanRenderer.hpp"

#include "EngineProperties.hpp"
#include "InputManager.hpp"
#include "VulkanDebug.hpp"
#include "VulkanGlTFModel.hpp"
#include "VulkanInitializers.hpp"
#include "VulkanTools.hpp"
#include "Window.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <ratio>
#include <string>
#include <vector>
#include <numeric>
#include <vulkan/vulkan_core.h>

VulkanRenderer::VulkanRenderer(EngineProperties* aEngineProperties,
	Window* aWindow)
	: mEngineProperties{aEngineProperties}
	, mWindow{aWindow}
	, mVkCommandBuffers{VK_NULL_HANDLE}
	, mTimer{0.0f}
	, mTimerSpeed{0.25f}
	, mIsPrepared{false}
	, mFramebufferWidth{0}
	, mFramebufferHeight{0}
	, mMaxFrametimes{10}
	, mFrametime{1.0f}
	, mGlTFModel{nullptr}
	, mVulkanDevice{nullptr}
	, mFrameCounter{0}
	, mLastFPS{0}
	, mAverageFrametime{0.0f}
	, mVkInstance{VK_NULL_HANDLE}
	, mVkQueue{VK_NULL_HANDLE}
	, mVkDepthFormat{VK_FORMAT_UNDEFINED}
	, mVkDescriptorPool{VK_NULL_HANDLE}
	, mVkPipelineCache{VK_NULL_HANDLE}
	, mBufferIndexCount{0}
	, mCurrentImageIndex{0}
	, mCurrentBufferIndex{0}
	, mVkPipelineLayout{VK_NULL_HANDLE}
	, mVkPipeline{VK_NULL_HANDLE}
	, mStarfieldVkPipeline{VK_NULL_HANDLE}
	, mVkDescriptorSetLayout{VK_NULL_HANDLE}
	, mVkCommandPoolBuffer{VK_NULL_HANDLE}
	, mVkPhysicalDevice13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES}
	, mModelPath{"Models/Voyager.gltf"}
	, mVertexShaderPath{"DynamicRendering/Texture_vert.spv"}
	, mFragmentShaderPath{"DynamicRendering/Texture_frag.spv"}
	, mStarfieldVertexShaderPath{"Instancing/Starfield_vert.spv"}
	, mStarfieldFragmentShaderPath{"Instancing/Starfield_frag.spv"}
{
	mEngineProperties->mAPIVersion = VK_API_VERSION_1_3;
	mEngineProperties->mIsValidationEnabled = true;
	mEngineProperties->mApplicationName = "Supernova";
	mEngineProperties->mEngineName = "Supernova";

	mFramebufferWidth = mEngineProperties->mWindowWidth;
	mFramebufferHeight = mEngineProperties->mWindowHeight;

	mVkPhysicalDevice13Features.dynamicRendering = VK_TRUE;
	mVkPhysicalDevice13Features.synchronization2 = VK_TRUE;

	// Setup a default look-at camera
	mCamera.SetType(CameraType::LookAt);
	mCamera.SetPosition(glm::vec3(0.0f, 0.0f, -10.0f));
	mCamera.SetRotation(glm::vec3(-7.5f, 72.0f, 0.0f));
	mCamera.SetPerspective(60.0f, static_cast<float>(mFramebufferWidth) / static_cast<float>(mFramebufferHeight), 1.0f, 256.0f);
}

VulkanRenderer::~VulkanRenderer()
{
	mVulkanSwapChain.CleanUp();

	if (mVulkanDevice->mLogicalVkDevice != VK_NULL_HANDLE)
	{
		if (mVkDescriptorPool != VK_NULL_HANDLE)
			vkDestroyDescriptorPool(mVulkanDevice->mLogicalVkDevice, mVkDescriptorPool, nullptr);

		vkFreeCommandBuffers(mVulkanDevice->mLogicalVkDevice, mVkCommandPoolBuffer, static_cast<std::uint32_t>(mVkCommandBuffers.size()), mVkCommandBuffers.data());

		for (VkShaderModule& shaderModule : mVkShaderModules)
			vkDestroyShaderModule(mVulkanDevice->mLogicalVkDevice, shaderModule, nullptr);

		vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImageView, nullptr);
		vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImage, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkDeviceMemory, nullptr);

		vkDestroyPipelineCache(mVulkanDevice->mLogicalVkDevice, mVkPipelineCache, nullptr);

		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mStarfieldVkPipeline, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipeline, nullptr);
		vkDestroyPipelineLayout(mVulkanDevice->mLogicalVkDevice, mVkPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, mVkDescriptorSetLayout, nullptr);
		vkDestroyCommandPool(mVulkanDevice->mLogicalVkDevice, mVkCommandPoolBuffer, nullptr);

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

	if (mEngineProperties->mIsValidationEnabled)
		VulkanDebug::DestroyDebugUtilsMessenger(mVkInstance);

	delete mGlTFModel;
	delete mVulkanDevice;

	vkDestroyInstance(mVkInstance, nullptr);
}

void VulkanRenderer::InitializeRenderer()
{
	InitializeVulkan();
	PrepareVulkanResources();
}

void VulkanRenderer::PrepareUpdate()
{
	mLastTimestamp = std::chrono::high_resolution_clock::now();
	mPreviousEndTime = mLastTimestamp;
}

void VulkanRenderer::UpdateRenderer(float /*aDeltaTime*/)
{
	if (mIsPrepared)
	{
		NextFrame();
	}

	mCamera.mKeys.mIsRightDown = InputManager::GetInstance().GetIsKeyDown(Key::Right);
	mCamera.mKeys.mIsUpDown = InputManager::GetInstance().GetIsKeyDown(Key::Up);
	mCamera.mKeys.mIsDownDown = InputManager::GetInstance().GetIsKeyDown(Key::Down);
	mCamera.mKeys.mIsLeftDown = InputManager::GetInstance().GetIsKeyDown(Key::Left);

	mCamera.Update(mFrametime);

	if (mVulkanDevice->mLogicalVkDevice != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(mVulkanDevice->mLogicalVkDevice);
	}

	mWindow->UpdateWindow(GetWindowTitle());
}

void VulkanRenderer::LoadAssets()
{
	const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
	mGlTFModel = new vkglTF::Model();
	mGlTFModel->LoadFromFile(VulkanTools::gResourcesPath / mModelPath, mVulkanDevice, mVkQueue, glTFLoadingFlags, 1.0f);
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
	// Allocate one command buffer per max. concurrent frame from above pool
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = VulkanInitializers::CommandBufferAllocateInfo(mVkCommandPoolBuffer, VK_COMMAND_BUFFER_LEVEL_PRIMARY, gMaxConcurrentFrames);
	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalVkDevice, &cmdBufAllocateInfo, mVkCommandBuffers.data()));
}

void VulkanRenderer::CreateDescriptors()
{
	// Pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, gMaxConcurrentFrames),
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = VulkanInitializers::descriptorPoolCreateInfo(poolSizes, gMaxConcurrentFrames);
	VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalVkDevice, &descriptorPoolInfo, nullptr, &mVkDescriptorPool));

	// Layout
	const std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
	};
	VkDescriptorSetLayoutCreateInfo descriptorLayout = VulkanInitializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, &descriptorLayout, nullptr, &mVkDescriptorSetLayout));

	// Sets per frame, just like the buffers themselves
	VkDescriptorSetAllocateInfo allocInfo = VulkanInitializers::descriptorSetAllocateInfo(mVkDescriptorPool, &mVkDescriptorSetLayout, 1);
	for (size_t i = 0; i < mVulkanUniformBuffers.size(); i++)
	{
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &allocInfo, &mVkDescriptorSets[i]));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			VulkanInitializers::writeDescriptorSet(mVkDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mVulkanUniformBuffers[i].mVkDescriptorBufferInfo),
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
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
	vkMemoryAllocateInfo.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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

VkShaderModule VulkanRenderer::LoadSPIRVShader(const std::filesystem::path& aPath) const
{
	size_t shaderSize{0};
	char* shaderCode{nullptr};

	std::ifstream is(aPath, std::ios::binary | std::ios::in | std::ios::ate);

	if (is.is_open())
	{
		shaderSize = is.tellg();
		is.seekg(0, std::ios::beg);
		// Copy file contents into a buffer
		shaderCode = new char[shaderSize];
		is.read(shaderCode, shaderSize);
		is.close();
		if (shaderSize <= 0)
		{
			throw std::runtime_error("Incorrect shader size");
		}
	}

	if (shaderCode)
	{
		// Create a new shader module that will be used for pipeline creation
		VkShaderModuleCreateInfo shaderModuleCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		shaderModuleCI.codeSize = shaderSize;
		shaderModuleCI.pCode = reinterpret_cast<std::uint32_t*>(shaderCode);

		VkShaderModule shaderModule;
		VK_CHECK_RESULT(vkCreateShaderModule(mVulkanDevice->mLogicalVkDevice, &shaderModuleCI, nullptr, &shaderModule));

		delete[] shaderCode;

		return shaderModule;
	}
	else
	{
		std::cerr << "Error: Could not open shader file \"" << aPath.generic_string() << "\"" << std::endl;
		return VK_NULL_HANDLE;
	}
}

void VulkanRenderer::CreatePipeline()
{
	// Layout
	// Uses set 0 for passing vertex shader ubo and set 1 for fragment shader images (taken from glTF model)
	const std::vector<VkDescriptorSetLayout> setLayouts = {
		mVkDescriptorSetLayout,
		vkglTF::descriptorSetLayoutImage,
	};
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VulkanInitializers::pipelineLayoutCreateInfo(setLayouts.data(), 2);
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalVkDevice, &pipelineLayoutCreateInfo, nullptr, &mVkPipelineLayout));

	// Pipeline
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = VulkanInitializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationState = VulkanInitializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentState = VulkanInitializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendState = VulkanInitializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = VulkanInitializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportState = VulkanInitializers::pipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleState = VulkanInitializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState = VulkanInitializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

	// We no longer need to set a renderpass for the pipeline create info
	VkGraphicsPipelineCreateInfo pipelineCI = VulkanInitializers::pipelineCreateInfo();
	pipelineCI.layout = mVkPipelineLayout;
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();
	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV});

	// New create info to define color, depth and stencil attachments at pipeline create time
	VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &mVulkanSwapChain.mColorVkFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = mVkDepthFormat;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = mVkDepthFormat;
	pipelineCI.pNext = &pipelineRenderingCreateInfo;
	
	// Vertex input bindings
	std::vector<VkVertexInputBindingDescription> bindingDescriptions = {
		// Binding point 0: Mesh vertex layout description at per-vertex rate
		VulkanInitializers::vertexInputBindingDescription(0, sizeof(vkglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};

	std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
		// Per-vertex attributes
		// These are advanced for each vertex fetched by the vertex shader
		VulkanInitializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0), // Location 0: Position
		VulkanInitializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3), // Location 1: Normal
		VulkanInitializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6), // Location 2: Texture coordinates
	};

	VkPipelineVertexInputStateCreateInfo inputState = VulkanInitializers::pipelineVertexInputStateCreateInfo();
	inputState.pVertexBindingDescriptions = bindingDescriptions.data();
	inputState.pVertexAttributeDescriptions = attributeDescriptions.data();

	pipelineCI.pVertexInputState = &inputState;

	shaderStages[0] = LoadShader(VulkanTools::gShadersPath / mVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(VulkanTools::gShadersPath / mFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputState.vertexBindingDescriptionCount = 1;
	inputState.vertexAttributeDescriptionCount = 3;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mVkPipelineCache, 1, &pipelineCI, nullptr, &mVkPipeline));

	rasterizationState.cullMode = VK_CULL_MODE_NONE;
	depthStencilState.depthWriteEnable = VK_FALSE;
	shaderStages[0] = LoadShader(VulkanTools::gShadersPath / mStarfieldVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(VulkanTools::gShadersPath / mStarfieldFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputState.vertexBindingDescriptionCount = 0;
	inputState.vertexAttributeDescriptionCount = 0;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mVkPipelineCache, 1, &pipelineCI, nullptr, &mStarfieldVkPipeline));
}

void VulkanRenderer::CreateUniformBuffers()
{
	for (VulkanBuffer& buffer : mVulkanUniformBuffers)
	{
		VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer, sizeof(VulkanUniformData), &mVulkanUniformData));
		VK_CHECK_RESULT(buffer.Map(VK_WHOLE_SIZE, 0));
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

	LoadAssets();
	CreateUniformBuffers();
	CreateDescriptors();
	CreatePipeline();

	mIsPrepared = true;
}

void VulkanRenderer::PrepareFrame()
{
	// Use a fence to wait until the command buffer has finished execution before using it again
	VK_CHECK_RESULT(vkWaitForFences(mVulkanDevice->mLogicalVkDevice, 1, &mWaitVkFences[mCurrentBufferIndex], VK_TRUE, UINT64_MAX));
	VK_CHECK_RESULT(vkResetFences(mVulkanDevice->mLogicalVkDevice, 1, &mWaitVkFences[mCurrentBufferIndex]));

	// By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
	// With that we don't have to handle VK_NOT_READY
	VkResult result = vkAcquireNextImageKHR(mVulkanDevice->mLogicalVkDevice, mVulkanSwapChain.mVkSwapchainKHR, UINT64_MAX, mVkPresentCompleteSemaphores[mCurrentBufferIndex], VK_NULL_HANDLE, &mCurrentImageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		OnResizeWindow();
		return;
	}
	else if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR))
	{
		throw std::runtime_error("Could not acquire the next swap chain image!");
	}
}

void VulkanRenderer::BuildCommandBuffer()
{
	VkCommandBuffer cmdBuffer = mVkCommandBuffers[mCurrentBufferIndex];

	VkCommandBufferBeginInfo cmdBufInfo = VulkanInitializers::commandBufferBeginInfo();
	VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

	// With dynamic rendering there are no subpass dependencies, so we need to take care of proper layout transitions by using barriers
	// This set of barriers prepares the color and depth images for output
	VulkanTools::InsertImageMemoryBarrier(
		cmdBuffer,
		mVulkanSwapChain.mVkImages[mCurrentImageIndex],
		0,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

	VulkanTools::InsertImageMemoryBarrier(
		cmdBuffer,
		mVulkanDepthStencil.mVkImage,
		0,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1});

	// New structures are used to define the attachments used in dynamic rendering
	VkRenderingAttachmentInfoKHR colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
	colorAttachment.imageView = mVulkanSwapChain.mVkImageViews[mCurrentImageIndex];
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue.color = {0.0f,0.0f,0.0f,0.0f};

	// A single depth stencil attachment info can be used, but they can also be specified separately.
	// When both are specified separately, the only requirement is that the image view is identical.			
	VkRenderingAttachmentInfoKHR depthStencilAttachment{};
	depthStencilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
	depthStencilAttachment.imageView = mVulkanDepthStencil.mVkImageView;
	depthStencilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthStencilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthStencilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthStencilAttachment.clearValue.depthStencil = {1.0f,  0};

	VkRenderingInfoKHR renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
	renderingInfo.renderArea = {0, 0, mFramebufferWidth, mFramebufferHeight};
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = &depthStencilAttachment;
	renderingInfo.pStencilAttachment = &depthStencilAttachment;

	// Begin dynamic rendering
	vkCmdBeginRendering(cmdBuffer, &renderingInfo);

	VkViewport viewport = VulkanInitializers::viewport((float)mFramebufferWidth, (float)mFramebufferHeight, 0.0f, 1.0f);
	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

	VkRect2D scissor = VulkanInitializers::rect2D(mFramebufferWidth, mFramebufferHeight, 0, 0);
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelineLayout, 0, 1, &mVkDescriptorSets[mCurrentBufferIndex], 0, nullptr);

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mStarfieldVkPipeline);
	vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipeline);
	mGlTFModel->Draw(cmdBuffer, vkglTF::RenderFlags::BindImages, mVkPipelineLayout, 1);

	// End dynamic rendering
	vkCmdEndRendering(cmdBuffer);

	// This set of barriers prepares the color image for presentation, we don't need to care for the depth image
	VulkanTools::InsertImageMemoryBarrier(
		cmdBuffer,
		mVulkanSwapChain.mVkImages[mCurrentImageIndex],
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		0,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

	VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));
}

void VulkanRenderer::UpdateUniformBuffers()
{
	mVulkanUniformData.mProjectionMatrix = mCamera.mMatrices.mPerspective;
	mVulkanUniformData.mModelViewMatrix = mCamera.mMatrices.mView;
	mVulkanUniformData.mViewPosition = mCamera.getViewPosition();
	std::memcpy(mVulkanUniformBuffers[mCurrentBufferIndex].mMappedData, &mVulkanUniformData, sizeof(VulkanUniformData));
}

void VulkanRenderer::SubmitFrame()
{
	const VkPipelineStageFlags waitPipelineStage{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &mVkPresentCompleteSemaphores[mCurrentBufferIndex],
		.pWaitDstStageMask = &waitPipelineStage,
		.commandBufferCount = 1,
		.pCommandBuffers = &mVkCommandBuffers[mCurrentBufferIndex],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &mVkRenderCompleteSemaphores[mCurrentImageIndex]
	};
	VK_CHECK_RESULT(vkQueueSubmit(mVkQueue, 1, &submitInfo, mWaitVkFences[mCurrentBufferIndex]));

	VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &mVkRenderCompleteSemaphores[mCurrentImageIndex],
		.swapchainCount = 1,
		.pSwapchains = &mVulkanSwapChain.mVkSwapchainKHR,
		.pImageIndices = &mCurrentImageIndex
	};

	VkResult result = vkQueuePresentKHR(mVkQueue, &presentInfo);
	// Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
	if (result == VK_ERROR_OUT_OF_DATE_KHR || mEngineProperties->mIsFramebufferResized)
	{
		mEngineProperties->mIsFramebufferResized = false;

		if (!mEngineProperties->mIsMinimized)
			OnResizeWindow();

		return;
	}
	else if ((result != VK_SUCCESS) && (result != VK_SUBOPTIMAL_KHR))
	{
		throw std::runtime_error("Could not acquire the next swap chain image!");
	}

	// Select the next frame to render to, based on the max. no. of concurrent frames
	mCurrentBufferIndex = (mCurrentBufferIndex + 1) % gMaxConcurrentFrames;
}

void VulkanRenderer::CreateVkInstance()
{
	mRequestedInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	mRequestedInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

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

	if (!mRequestedInstanceExtensions.empty())
	{
		for (const char* requestedExtension : mRequestedInstanceExtensions)
		{
			if (std::find(mSupportedInstanceExtensions.begin(), mSupportedInstanceExtensions.end(), requestedExtension) == mSupportedInstanceExtensions.end())
			{
				std::cerr << "Requested instance extension \"" << requestedExtension << "\" is not present at instance level" << std::endl;
				continue;
			}

			mInstanceExtensions.push_back(requestedExtension);
		}
	}

	// Shaders generated by Slang require a certain SPIR-V environment that can't be satisfied by Vulkan 1.0, so we need to expliclity up that to at least 1.1 and enable some required extensions
	if (VulkanTools::gShaderType == VulkanTools::ShaderType::Slang)
	{
		if (mEngineProperties->mAPIVersion < VK_API_VERSION_1_1)
		{
			mEngineProperties->mAPIVersion = VK_API_VERSION_1_1;
		}
		mEnabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
		mEnabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
		mEnabledDeviceExtensions.push_back(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);
	}

	VkApplicationInfo vkApplicationInfo{};
	vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vkApplicationInfo.pApplicationName = mEngineProperties->mApplicationName.c_str();
	vkApplicationInfo.pEngineName = mEngineProperties->mEngineName.c_str();
	vkApplicationInfo.apiVersion = mEngineProperties->mAPIVersion;

	VkInstanceCreateInfo vkInstanceCreateInfo{};
	vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;

	VkDebugUtilsMessengerCreateInfoEXT vkDebugUtilsMessengerCreateInfo{};
	if (mEngineProperties->mIsValidationEnabled)
	{
		VulkanDebug::SetupDebugingMessengerCreateInfo(vkDebugUtilsMessengerCreateInfo);
		vkDebugUtilsMessengerCreateInfo.pNext = vkInstanceCreateInfo.pNext;
		vkInstanceCreateInfo.pNext = &vkDebugUtilsMessengerCreateInfo;
	}

	if (mEngineProperties->mIsValidationEnabled || std::find(mSupportedInstanceExtensions.begin(), mSupportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != mSupportedInstanceExtensions.end())
		mInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	const std::vector<const char*> glfwRequiredExtensions = mWindow->GetGlfwRequiredExtensions();
	for (const char* glfwRequiredExtension : glfwRequiredExtensions)
	{
		auto iterator = std::ranges::find_if(mInstanceExtensions, [&](const char* aInstanceExtension)
		{
			return std::strcmp(aInstanceExtension, glfwRequiredExtension) == 0;
		});

		if (iterator == mInstanceExtensions.end())
		{
			mInstanceExtensions.push_back(glfwRequiredExtension);
		}
	}

	if (!mInstanceExtensions.empty())
	{
		vkInstanceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(mInstanceExtensions.size());
		vkInstanceCreateInfo.ppEnabledExtensionNames = mInstanceExtensions.data();

#ifndef NDEBUG
		for (const char* instanceExtension : mInstanceExtensions)
		{
			std::cout << "Enabling instance extension " << instanceExtension << std::endl;
		}
#endif
	}

	if (mEngineProperties->mIsValidationEnabled)
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
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error(std::format("Could not create Vulkan instance: {}", VulkanTools::GetErrorString(result)));
	}

	mWindow->CreateWindowSurface(&mVkInstance, &mVulkanSwapChain.mVkSurfaceKHR);

	// If the debug utils extension is present we set up debug functions, so samples can label objects for debugging
	if (std::find(mSupportedInstanceExtensions.begin(), mSupportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != mSupportedInstanceExtensions.end())
	{
		VulkanDebug::SetupDebugUtils(mVkInstance);
	}
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
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(mVkInstance, &physicalDeviceCount, vkPhysicalDevices.data()));

	std::uint32_t selectedDevice = 0;
	VkPhysicalDevice vkPhysicalDevice = vkPhysicalDevices[selectedDevice];
	mVulkanDevice = new VulkanDevice();
	mVulkanDevice->CreatePhysicalDevice(vkPhysicalDevice);
	mVulkanDevice->CreateLogicalDevice(mEnabledDeviceExtensions, &mVkPhysicalDevice13Features, true, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
}

std::string VulkanRenderer::GetWindowTitle() const
{
	return std::format("{} - {} - {:.3f} ms {} fps - {}/{} window - {}/{} framebuffer",
		mEngineProperties->mApplicationName,
		mVulkanDevice->mVkPhysicalDeviceProperties.deviceName,
		(mAverageFrametime * 1000.f),
		mLastFPS,
		mEngineProperties->mWindowWidth,
		mEngineProperties->mWindowHeight,
		mFramebufferWidth,
		mFramebufferHeight);
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

VkPipelineShaderStageCreateInfo VulkanRenderer::LoadShader(const std::filesystem::path& aPath, VkShaderStageFlagBits aVkShaderStageMask)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = aVkShaderStageMask;
	shaderStage.module = VulkanTools::LoadShader(aPath, mVulkanDevice->mLogicalVkDevice);
	shaderStage.pName = "main";

	if (shaderStage.module == VK_NULL_HANDLE)
	{
		throw std::runtime_error("Incorrect shader module");
	}

	mVkShaderModules.push_back(shaderStage.module);
	return shaderStage;
}

void VulkanRenderer::NextFrame()
{
	const std::chrono::steady_clock::time_point frameTimeStart = std::chrono::high_resolution_clock::now();
	
	PrepareFrame();
	UpdateUniformBuffers();
	BuildCommandBuffer();
	SubmitFrame();

	mFrameCounter++;
	const std::chrono::steady_clock::time_point frameTimeEnd = std::chrono::high_resolution_clock::now();
	const float frameTimeDelta = std::chrono::duration<float, std::milli>(frameTimeEnd - frameTimeStart).count();
	mFrametime = frameTimeDelta / 1000.0f;

	mFrametimes.push_back(mFrametime);
	if (mFrametimes.size() > mMaxFrametimes)
	{
		mFrametimes.erase(mFrametimes.begin());
	}

	const float fpsTimer = std::chrono::duration<float, std::milli>(frameTimeEnd - mLastTimestamp).count();
	if (fpsTimer > 1000.0f)
	{
		mLastFPS = static_cast<std::uint32_t>(static_cast<float>(mFrameCounter) * (1000.0f / fpsTimer));
		const float frameTimeSum = std::accumulate(mFrametimes.begin(), mFrametimes.end(), 0.0f);
		mAverageFrametime = frameTimeSum / mFrametimes.size();
		mFrameCounter = 0;
		mLastTimestamp = frameTimeEnd;
	}

	mPreviousEndTime = frameTimeEnd;
}

void VulkanRenderer::InitializeVulkan()
{
	CreateVkInstance();

	// If requested, we enable the default validation layers for debugging
	if (mEngineProperties->mIsValidationEnabled)
	{
		VulkanDebug::SetupDebugUtilsMessenger(mVkInstance);
	}

	CreateVulkanDevice();

	// Get a graphics queue from the device
	vkGetDeviceQueue(mVulkanDevice->mLogicalVkDevice, mVulkanDevice->mQueueFamilyIndices.mGraphics, 0, &mVkQueue);

	// Applications that make use of stencil will require a depth + stencil format
	const VkBool32 validFormat = VulkanTools::GetSupportedDepthFormat(mVulkanDevice->mVkPhysicalDevice, &mVkDepthFormat);
	if (!validFormat)
	{
		throw std::runtime_error("Invalid format");
	}

	mVulkanSwapChain.SetContext(mVkInstance, mVulkanDevice);
}

void VulkanRenderer::CreateCommandPool()
{
	VkCommandPoolCreateInfo vkCommandPoolCreateInfo = {};
	vkCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	vkCommandPoolCreateInfo.queueFamilyIndex = mVulkanSwapChain.mQueueNodeIndex;
	vkCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(mVulkanDevice->mLogicalVkDevice, &vkCommandPoolCreateInfo, nullptr, &mVkCommandPoolBuffer));
}

void VulkanRenderer::OnResizeWindow()
{
	if (!mIsPrepared)
		return;
	
	mIsPrepared = false;

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
		mCamera.UpdateAspectRatio(static_cast<float>(mFramebufferWidth) / static_cast<float>(mFramebufferHeight));
	}

	mIsPrepared = true;
}

void VulkanRenderer::SetupSwapchain()
{
	mVulkanSwapChain.CreateSwapchain(mFramebufferWidth, mFramebufferHeight, mEngineProperties->mIsVSyncEnabled);
}
