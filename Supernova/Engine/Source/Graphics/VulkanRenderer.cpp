#include "VulkanRenderer.hpp"

#include "Camera.hpp"
#include "EngineProperties.hpp"
#include "FileLoader.hpp"
#include "ImGuiOverlay.hpp"
#include "Input/InputManager.hpp"
#include "Time.hpp"
#include "Timer.hpp"
#include "VulkanDebug.hpp"
#include "VulkanGlTFModel.hpp"
#include "VulkanInitializers.hpp"
#include "VulkanTools.hpp"
#include "VulkanTypes.hpp"
#include "Window.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <imgui.h>
#include <filesystem>
#include <random>
#include <numbers>
#include <cmath>
#include <cstddef>

VulkanRenderer::VulkanRenderer(EngineProperties* aEngineProperties,
	Window* aWindow)
	: mEngineProperties{aEngineProperties}
	, mWindow{aWindow}
	, mVkCommandBuffers{VK_NULL_HANDLE}
	, mFramebufferWidth{0}
	, mFramebufferHeight{0}
	, mFrametime{1.0f}
	, mVulkanDevice{nullptr}
	, mImGuiOverlay{nullptr}
	, mCamera{nullptr}
	, mFrameTimer{nullptr}
	, mFrameCounter{0}
	, mAverageFPS{0}
	, mFPSTimerInterval{1000.0f}
	, mVkInstance{VK_NULL_HANDLE}
	, mVkQueue{VK_NULL_HANDLE}
	, mVkDepthFormat{VK_FORMAT_UNDEFINED}
	, mVkDescriptorPool{VK_NULL_HANDLE}
	, mVkPipelineCache{VK_NULL_HANDLE}
	, mBufferIndexCount{0}
	, mCurrentImageIndex{0}
	, mCurrentBufferIndex{0}
	, mIndirectDrawCount{0}
	, mIndirectInstanceCount{0}
	, mVkPipelineLayout{VK_NULL_HANDLE}
	, mVkDescriptorSetLayout{VK_NULL_HANDLE}
	, mVkCommandPoolBuffer{VK_NULL_HANDLE}
	, mVkPhysicalDevice13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES}
	, mVoyagerModelMatrix{1.0f}
	, mPlanetModelMatrix{1.0f}
{
	mFrameTimer = new Time::Timer();
	
	mEngineProperties->mAPIVersion = VK_API_VERSION_1_3;
	mEngineProperties->mIsValidationEnabled = true;
	mEngineProperties->mIsVSyncEnabled = true;

	mFramebufferWidth = mEngineProperties->mWindowWidth;
	mFramebufferHeight = mEngineProperties->mWindowHeight;

	mVkPhysicalDevice13Features.dynamicRendering = VK_TRUE;
	mVkPhysicalDevice13Features.synchronization2 = VK_TRUE;

	mImGuiOverlay = new ImGuiOverlay();

	// Setup a default look-at camera
	mCamera = new Camera();
	mCamera->SetType(CameraType::LookAt);
	mCamera->SetPosition(glm::vec3(5.5f, -1.85f, -18.5f));
	mCamera->SetRotation(glm::vec3(-17.2f, -4.7f, 0.0f));
	mCamera->SetRotationSpeed(2.0f);
	mCamera->SetZoomSpeed(15.0f);
	mCamera->SetPerspective(60.0f, static_cast<float>(mFramebufferWidth) / static_cast<float>(mFramebufferHeight), 0.1f, 256.0f);

	mVoyagerModelMatrix = glm::translate(mVoyagerModelMatrix, glm::vec3{1.0f, -2.0f, 10.0f});
	mVoyagerModelMatrix = glm::scale(mVoyagerModelMatrix, glm::vec3{0.2f});
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

		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipelines.mPlanet, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipelines.mRocks, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipelines.mStarfield, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipelines.mVoyager, nullptr);
		vkDestroyPipelineLayout(mVulkanDevice->mLogicalVkDevice, mVkPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, mVkDescriptorSetLayout, nullptr);
		vkDestroyCommandPool(mVulkanDevice->mLogicalVkDevice, mVkCommandPoolBuffer, nullptr);

		mInstanceBuffer.Destroy();
		mIndirectCommandsBuffer.Destroy();

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

		mTextures.mRockTextureArray.Destroy();
		mTextures.mPlanetTexture.Destroy();
	}

	mImGuiOverlay->FreeResources();

	if (mEngineProperties->mIsValidationEnabled)
		VulkanDebug::DestroyDebugUtilsMessenger(mVkInstance);

	delete mModels.mPlanetModel;
	delete mModels.mRockModel;
	delete mModels.mVoyagerModel;
	delete mFrameTimer;
	delete mImGuiOverlay;
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
	mLastTimestamp = std::chrono::steady_clock::now();
}

void VulkanRenderer::EndUpdate()
{
	if (mVulkanDevice->mLogicalVkDevice != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(mVulkanDevice->mLogicalVkDevice);
	}
}

void VulkanRenderer::UpdateRenderer(float /*aDeltaTime*/)
{
	if (!mEngineProperties->mIsMinimized)
	{
		if (mEngineProperties->mIsRendererPrepared)
		{
			RenderFrame();
		}

		Input::InputManager& inputManager = Input::InputManager::GetInstance();
		mCamera->mKeys.mIsRightDown = inputManager.GetIsKeyDown(Input::Key::Right) || inputManager.GetIsKeyDown(Input::Key::D);
		mCamera->mKeys.mIsUpDown = inputManager.GetIsKeyDown(Input::Key::Up) || inputManager.GetIsKeyDown(Input::Key::W);
		mCamera->mKeys.mIsDownDown = inputManager.GetIsKeyDown(Input::Key::Down) || inputManager.GetIsKeyDown(Input::Key::S);
		mCamera->mKeys.mIsLeftDown = inputManager.GetIsKeyDown(Input::Key::Left) || inputManager.GetIsKeyDown(Input::Key::A);
		mCamera->mKeys.mIsShiftDown = inputManager.GetIsKeyDown(Input::Key::LeftShift);
		mCamera->mKeys.mIsSpaceDown = inputManager.GetIsKeyDown(Input::Key::Spacebar);
		mCamera->mKeys.mIsCtrlDown = inputManager.GetIsKeyDown(Input::Key::LeftControl);
		mCamera->mMouse.mScrollWheelDelta = inputManager.GetScrollOffset().y;
		mCamera->mMouse.mIsLeftDown = inputManager.GetIsMouseButtonDown(Input::MouseButtons::Left);
		mCamera->mMouse.mIsMiddleDown = inputManager.GetIsMouseButtonDown(Input::MouseButtons::Middle);
		mCamera->mMouse.mDeltaX = inputManager.GetMousePositionDelta().x;
		mCamera->mMouse.mDeltaY = inputManager.GetMousePositionDelta().y;

		inputManager.FlushInput(); // TODO: Fix this bad solution to having frame-based offsets

		mCamera->Update(mFrametime);
	}

	mWindow->UpdateWindow();
}

void VulkanRenderer::LoadAssets()
{
	const std::uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
	mModels.mVoyagerModel = new vkglTF::Model();
	const std::filesystem::path voyagerModelPath = "Voyager.gltf";
	mModels.mVoyagerModel->LoadFromFile(FileLoader::GetEngineResourcesPath() / FileLoader::gModelsPath / voyagerModelPath, mVulkanDevice, mVkQueue, glTFLoadingFlags, 1.0f);

	mModels.mRockModel = new vkglTF::Model();
	const std::filesystem::path rockModelPath = "Rock01.gltf";
	mModels.mRockModel->LoadFromFile(FileLoader::GetEngineResourcesPath() / FileLoader::gModelsPath / rockModelPath, mVulkanDevice, mVkQueue, glTFLoadingFlags, 1.0f);

	mModels.mPlanetModel = new vkglTF::Model();
	const std::filesystem::path planetModelPath = "Lavaplanet.gltf";
	mModels.mPlanetModel->LoadFromFile(FileLoader::GetEngineResourcesPath() / FileLoader::gModelsPath / planetModelPath, mVulkanDevice, mVkQueue, glTFLoadingFlags, 1.0f);

	const std::filesystem::path planetTexturePath = "Lavaplanet_rgba.ktx";
	mTextures.mPlanetTexture.LoadFromFile(FileLoader::GetEngineResourcesPath() / FileLoader::gTexturesPath / planetTexturePath, VK_FORMAT_R8G8B8A8_UNORM, mVulkanDevice, mVkQueue);
	const std::filesystem::path rockTexturePath = "Texturearray_rocks_rgba.ktx";
	mTextures.mRockTextureArray.LoadFromFile(FileLoader::GetEngineResourcesPath() / FileLoader::gTexturesPath / rockTexturePath, VK_FORMAT_R8G8B8A8_UNORM, mVulkanDevice, mVkQueue, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
	const VkCommandBufferAllocateInfo cmdBufAllocateInfo = VulkanInitializers::CommandBufferAllocateInfo(mVkCommandPoolBuffer, VK_COMMAND_BUFFER_LEVEL_PRIMARY, gMaxConcurrentFrames);
	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalVkDevice, &cmdBufAllocateInfo, mVkCommandBuffers.data()));
}

void VulkanRenderer::CreateDescriptorPool()
{
	static constexpr std::uint32_t poolPadding = 2;
	const std::vector<VkDescriptorPoolSize> poolSizes = {
		VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (gMaxConcurrentFrames * 3) + poolPadding),
		VulkanInitializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (gMaxConcurrentFrames * 2) + poolPadding),
	};
	const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VulkanInitializers::descriptorPoolCreateInfo(poolSizes, gMaxConcurrentFrames * 3);
	VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalVkDevice, &descriptorPoolCreateInfo, nullptr, &mVkDescriptorPool));
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
	const std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0 : Vertex shader uniform buffer
		VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		// Binding 1 : Fragment shader combined sampler
		VulkanInitializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
	};
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = VulkanInitializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, &descriptorSetLayoutCreateInfo, nullptr, &mVkDescriptorSetLayout));
}

void VulkanRenderer::CreateDescriptorSets()
{
	// Sets per frame, just like the buffers themselves
	const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VulkanInitializers::descriptorSetAllocateInfo(mVkDescriptorPool, &mVkDescriptorSetLayout, 1);
	for (std::size_t i = 0; i < mVulkanUniformBuffers.size(); i++)
	{
		// Instanced models
		// Binding 0 : Vertex shader uniform buffer
		// Binding 1 : Color map
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocateInfo, &mVkDescriptorSets[i].mInstancedRocks));
		const std::vector<VkWriteDescriptorSet> instancedWriteDescriptorSets = {
			VulkanInitializers::writeDescriptorSet(mVkDescriptorSets[i].mInstancedRocks, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mVulkanUniformBuffers[i].mVkDescriptorBufferInfo),
			VulkanInitializers::writeDescriptorSet(mVkDescriptorSets[i].mInstancedRocks, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &mTextures.mRockTextureArray.mDescriptor),
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<std::uint32_t>(instancedWriteDescriptorSets.size()), instancedWriteDescriptorSets.data(), 0, nullptr);

		// Static planet
		//	Binding 0 : Vertex shader uniform buffer
		//	Binding 1 : Color map
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocateInfo, &mVkDescriptorSets[i].mStaticPlanetWithStarfield));
		const std::vector<VkWriteDescriptorSet> staticPlanetWriteDescriptorSets = {
			VulkanInitializers::writeDescriptorSet(mVkDescriptorSets[i].mStaticPlanetWithStarfield, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mVulkanUniformBuffers[i].mVkDescriptorBufferInfo),
			VulkanInitializers::writeDescriptorSet(mVkDescriptorSets[i].mStaticPlanetWithStarfield, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &mTextures.mPlanetTexture.mDescriptor),
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<std::uint32_t>(staticPlanetWriteDescriptorSets.size()), staticPlanetWriteDescriptorSets.data(), 0, nullptr);

		// Static voyager
		//	Binding 0 : Vertex shader uniform buffer
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocateInfo, &mVkDescriptorSets[i].mStaticVoyager));
		const std::vector<VkWriteDescriptorSet> staticVoyagerWriteDescriptorSets = {
			VulkanInitializers::writeDescriptorSet(mVkDescriptorSets[i].mStaticVoyager, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mVulkanUniformBuffers[i].mVkDescriptorBufferInfo),
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<std::uint32_t>(staticVoyagerWriteDescriptorSets.size()), staticVoyagerWriteDescriptorSets.data(), 0, nullptr);
	}
}

void VulkanRenderer::SetupDepthStencil()
{
	// Create an optimal tiled image used as the depth stencil attachment
	const VkImageCreateInfo vkImageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = mVkDepthFormat,
		.extent = { mFramebufferWidth, mFramebufferHeight, 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalVkDevice, &vkImageCreateInfo, nullptr, &mVulkanDepthStencil.mVkImage));

	// Allocate memory for the image (device local) and bind it to our image
	VkMemoryRequirements vkMemoryRequirements;
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImage, &vkMemoryRequirements);

	const VkMemoryAllocateInfo vkMemoryAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = vkMemoryRequirements.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(vkMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &vkMemoryAllocateInfo, nullptr, &mVulkanDepthStencil.mVkDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImage, mVulkanDepthStencil.mVkDeviceMemory, 0));

	// Create a view for the depth stencil image
	// Images aren't directly accessed in Vulkan, but rather through views described by a subresource range
	// This allows for multiple views of one image with differing ranges (e.g. for different layers)
	VkImageViewCreateInfo vkImageViewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = mVulkanDepthStencil.mVkImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = mVkDepthFormat,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		}
	};
	
	// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT)
	if (mVkDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
	{
		vkImageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalVkDevice, &vkImageViewCreateInfo, nullptr, &mVulkanDepthStencil.mVkImageView));
}

void VulkanRenderer::CreateGraphicsPipelines()
{
	// Layout
	// Uses set 0 for passing vertex shader ubo and set 1 for fragment shader images (taken from glTF model)
	const std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
		mVkDescriptorSetLayout,
		vkglTF::gDescriptorSetLayoutImage,
	};
	
	const VkPushConstantRange pushConstantRange{
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(VulkanPushConstant)
	};

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VulkanInitializers::pipelineLayoutCreateInfo(descriptorSetLayouts.data(), 2);
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalVkDevice, &pipelineLayoutCreateInfo, nullptr, &mVkPipelineLayout));

	// Pipeline
	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = VulkanInitializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationState = VulkanInitializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	const VkPipelineColorBlendAttachmentState blendAttachmentState = VulkanInitializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	const VkPipelineColorBlendStateCreateInfo colorBlendState = VulkanInitializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = VulkanInitializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	const VkPipelineViewportStateCreateInfo viewportState = VulkanInitializers::pipelineViewportStateCreateInfo(1, 1, 0);
	const VkPipelineMultisampleStateCreateInfo multisampleState = VulkanInitializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	const VkPipelineDynamicStateCreateInfo dynamicState = VulkanInitializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
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
	pipelineCI.stageCount = static_cast<std::uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();

	// New create info to define color, depth and stencil attachments at pipeline create time
	const VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &mVulkanSwapChain.mColorVkFormat,
		.depthAttachmentFormat = mVkDepthFormat,
		.stencilAttachmentFormat = mVkDepthFormat,
	};
	pipelineCI.pNext = &pipelineRenderingCreateInfo;
	
	// Vertex input bindings
	const std::vector<VkVertexInputBindingDescription> bindingDescriptions = {
		// Binding point 0: Mesh vertex layout description at per-vertex rate
		VulkanInitializers::vertexInputBindingDescription(0, sizeof(vkglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
		// Binding point 1: Instanced data at per-instance rate
		VulkanInitializers::vertexInputBindingDescription(1, sizeof(VulkanInstanceData), VK_VERTEX_INPUT_RATE_INSTANCE),
	};

	const std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
		// Per-vertex attributes
		// These are advanced for each vertex fetched by the vertex shader
		VulkanInitializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, mPosition)), // Location 0: Position
		VulkanInitializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, mNormal)), // Location 1: Normal
		VulkanInitializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(vkglTF::Vertex, mUV)), // Location 2: Texture coordinates
		VulkanInitializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, mColor)), // Location 3: Color
		// Per-Instance attributes
		// These are advanced for each instance rendered
		VulkanInitializers::vertexInputAttributeDescription(1, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanInstanceData, mPosition)), // Location 4: Position
		VulkanInitializers::vertexInputAttributeDescription(1, 5, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanInstanceData, mRotation)), // Location 5: Rotation
		VulkanInitializers::vertexInputAttributeDescription(1, 6, VK_FORMAT_R32_SFLOAT, offsetof(VulkanInstanceData, mScale)), // Location 6: Scale
		VulkanInitializers::vertexInputAttributeDescription(1, 7, VK_FORMAT_R32_SINT, offsetof(VulkanInstanceData, mTextureIndex)), // Location 7: Texture array layer index
	};

	VkPipelineVertexInputStateCreateInfo inputState = VulkanInitializers::pipelineVertexInputStateCreateInfo();
	inputState.pVertexBindingDescriptions = bindingDescriptions.data();
	inputState.pVertexAttributeDescriptions = attributeDescriptions.data();

	pipelineCI.pVertexInputState = &inputState;

	const std::filesystem::path voyagerVertexShaderPath = "DynamicRendering/Texture_vert.spv";
	const std::filesystem::path voyagerFragmentShaderPath = "DynamicRendering/Texture_frag.spv";
	shaderStages[0] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / voyagerVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / voyagerFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputState.vertexBindingDescriptionCount = 1;
	inputState.vertexAttributeDescriptionCount = 3;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mVkPipelineCache, 1, &pipelineCI, nullptr, &mVkPipelines.mVoyager));

	const std::filesystem::path planetVertexShaderPath = "Instancing/Planet_vert.spv";
	const std::filesystem::path planetFragmentShaderPath = "Instancing/Planet_frag.spv";
	shaderStages[0] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / planetVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / planetFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputState.vertexBindingDescriptionCount = 1;
	inputState.vertexAttributeDescriptionCount = 4;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mVkPipelineCache, 1, &pipelineCI, nullptr, &mVkPipelines.mPlanet));

	const std::filesystem::path rockVertexShaderPath = "Instancing/Instancing_vert.spv";
	const std::filesystem::path rockFragmentShaderPath = "Instancing/Instancing_frag.spv";
	shaderStages[0] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / rockVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / rockFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputState.vertexBindingDescriptionCount = static_cast<std::uint32_t>(bindingDescriptions.size());
	inputState.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mVkPipelineCache, 1, &pipelineCI, nullptr, &mVkPipelines.mRocks));

	rasterizationState.cullMode = VK_CULL_MODE_NONE;
	depthStencilState.depthWriteEnable = VK_FALSE;
	const std::filesystem::path starfieldVertexShaderPath = "Instancing/Starfield_vert.spv";
	const std::filesystem::path starfieldFragmentShaderPath = "Instancing/Starfield_frag.spv";
	shaderStages[0] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / starfieldVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / starfieldFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputState.vertexBindingDescriptionCount = 0;
	inputState.vertexAttributeDescriptionCount = 0;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mVkPipelineCache, 1, &pipelineCI, nullptr, &mVkPipelines.mStarfield));
}

void VulkanRenderer::CreateUniformBuffers()
{
	for (VulkanBuffer& buffer : mVulkanUniformBuffers)
	{
		VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer, sizeof(VulkanUniformData), &mVulkanUniformData));
		VK_CHECK_RESULT(buffer.Map());
	}
}

void VulkanRenderer::CreateUIOverlay()
{
	const std::filesystem::path UIVertexShaderPath = "Core/UIOverlay_vert.spv";
	const std::filesystem::path UIFragmentShaderPath = "Core/UIOverlay_frag.spv";
	mImGuiOverlay->SetMaxConcurrentFrames(gMaxConcurrentFrames);
	mImGuiOverlay->SetVulkanDevice(mVulkanDevice);
	mImGuiOverlay->SetVkQueue(mVkQueue);
	mImGuiOverlay->AddShader(LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / UIVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT));
	mImGuiOverlay->AddShader(LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / UIFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT));
	mImGuiOverlay->PrepareResources();
	mImGuiOverlay->PreparePipeline(mVkPipelineCache, mVulkanSwapChain.mColorVkFormat, mVkDepthFormat);
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
	
	CreateUIOverlay();

	LoadAssets();
	
	PrepareIndirectData();
	PrepareInstanceData();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateDescriptorSetLayout();
	CreateDescriptorSets();
	CreateGraphicsPipelines();

	mEngineProperties->mIsRendererPrepared = true;
}

void VulkanRenderer::PrepareFrame()
{
	// Use a fence to wait until the command buffer has finished execution before using it again
	VK_CHECK_RESULT(vkWaitForFences(mVulkanDevice->mLogicalVkDevice, 1, &mWaitVkFences[mCurrentBufferIndex], VK_TRUE, UINT64_MAX));
	VK_CHECK_RESULT(vkResetFences(mVulkanDevice->mLogicalVkDevice, 1, &mWaitVkFences[mCurrentBufferIndex]));

	UpdateUIOverlay();

	// By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
	// With that we don't have to handle VK_NOT_READY
	const VkResult result = vkAcquireNextImageKHR(mVulkanDevice->mLogicalVkDevice, mVulkanSwapChain.mVkSwapchainKHR, UINT64_MAX, mVkPresentCompleteSemaphores[mCurrentBufferIndex], VK_NULL_HANDLE, &mCurrentImageIndex);
	if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
	{
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			OnResizeWindow();
		}

		return;
	}
	else
	{
		VK_CHECK_RESULT(result);
	}
}

void VulkanRenderer::BuildGraphicsCommandBuffer()
{
	VkCommandBuffer commandBuffer = mVkCommandBuffers[mCurrentBufferIndex];

	const VkCommandBufferBeginInfo commandBufferBeginInfo = VulkanInitializers::commandBufferBeginInfo();
	VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

	// With dynamic rendering there are no subpass dependencies, so we need to take care of proper layout transitions by using barriers
	// This set of barriers prepares the color and depth images for output
	VulkanTools::InsertImageMemoryBarrier(
		commandBuffer,
		mVulkanSwapChain.mVkImages[mCurrentImageIndex],
		0,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

	VulkanTools::InsertImageMemoryBarrier(
		commandBuffer,
		mVulkanDepthStencil.mVkImage,
		0,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1});

	// New structures are used to define the attachments used in dynamic rendering
	const VkRenderingAttachmentInfoKHR colorAttachmentInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
		.imageView = mVulkanSwapChain.mVkImageViews[mCurrentImageIndex],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { .color = {0.0f,0.0f,0.0f,0.0f} }
	};

	// A single depth stencil attachment info can be used, but they can also be specified separately.
	// When both are specified separately, the only requirement is that the image view is identical.			
	const VkRenderingAttachmentInfoKHR depthStencilAttachmentInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
		.imageView = mVulkanDepthStencil.mVkImageView,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { .depthStencil = {1.0f, 0} }
	};

	const VkRenderingInfoKHR renderingInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		.renderArea = {0, 0, mFramebufferWidth, mFramebufferHeight},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentInfo,
		.pDepthAttachment = &depthStencilAttachmentInfo,
		.pStencilAttachment = &depthStencilAttachmentInfo
	};

	// Begin dynamic rendering
	vkCmdBeginRendering(commandBuffer, &renderingInfo);

	VkViewport viewport = VulkanInitializers::viewport(static_cast<float>(mFramebufferWidth), static_cast<float>(mFramebufferHeight), 0.0f, 1.0f);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = VulkanInitializers::rect2D(mFramebufferWidth, mFramebufferHeight, 0, 0);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// Draw non-instanced static models
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelineLayout, 0, 1, &mVkDescriptorSets[mCurrentBufferIndex].mStaticPlanetWithStarfield, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelines.mStarfield);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelines.mPlanet);

	mVulkanPushConstant.mModelMatrix = mPlanetModelMatrix;
	vkCmdPushConstants(commandBuffer, mVkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VulkanPushConstant), &mVulkanPushConstant);

	mModels.mPlanetModel->Draw(commandBuffer);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelineLayout, 0, 1, &mVkDescriptorSets[mCurrentBufferIndex].mStaticVoyager, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelines.mVoyager);

	mVulkanPushConstant.mModelMatrix = mVoyagerModelMatrix;
	vkCmdPushConstants(commandBuffer, mVkPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VulkanPushConstant), &mVulkanPushConstant);

	mModels.mVoyagerModel->Draw(commandBuffer, vkglTF::RenderFlags::BindImages, mVkPipelineLayout);

	// Draw instanced multi draw models
	VkDeviceSize offsets[1] = {0};
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelineLayout, 0, 1, &mVkDescriptorSets[mCurrentBufferIndex].mInstancedRocks, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelines.mRocks);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mModels.mRockModel->vertices.mBuffer, offsets);
	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &mInstanceBuffer.mVkBuffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, mModels.mRockModel->indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
	
	// One draw call for an arbitrary number of objects
	if (mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.multiDrawIndirect)
	{
		// Index offsets and instance count are taken from the indirect buffer
		vkCmdDrawIndexedIndirect(commandBuffer, mIndirectCommandsBuffer.mVkBuffer, 0, mIndirectDrawCount, sizeof(VkDrawIndexedIndirectCommand));
	}
	else
	{
		// Issue separate draw commands
		for (std::size_t j = 0; j < mDrawIndexedIndirectCommands.size(); j++)
		{
			vkCmdDrawIndexedIndirect(commandBuffer, mIndirectCommandsBuffer.mVkBuffer, j * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
		}
	}

	DrawImGuiOverlay(commandBuffer);

	// End dynamic rendering
	vkCmdEndRendering(commandBuffer);

	// This set of barriers prepares the color image for presentation, we don't need to care for the depth image
	VulkanTools::InsertImageMemoryBarrier(
		commandBuffer,
		mVulkanSwapChain.mVkImages[mCurrentImageIndex],
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		0,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
}

void VulkanRenderer::UpdateModelMatrix()
{
	const glm::vec3 pivotPoint = glm::vec3{20.0f, 0.0f, 80.0f};
	mVoyagerModelMatrix = glm::translate(mVoyagerModelMatrix, -pivotPoint);

	static constexpr float angle = glm::radians(-5.0f);
	glm::vec3 rotationAxis = glm::vec3{0.0f, 1.0f, 0.0f};
	mVoyagerModelMatrix = glm::rotate(mVoyagerModelMatrix, angle * mFrametime, rotationAxis);

	mVoyagerModelMatrix = glm::translate(mVoyagerModelMatrix, pivotPoint);
}

void VulkanRenderer::UpdateUniformBuffers()
{
	mVulkanUniformData.mProjectionMatrix = mCamera->mMatrices.mPerspective;
	mVulkanUniformData.mViewMatrix = mCamera->mMatrices.mView;
	mVulkanUniformData.mViewPosition = mCamera->GetViewPosition();
	mVulkanUniformData.mLocalSpeed += mFrametime * 0.35f;
	mVulkanUniformData.mGlobalSpeed += mFrametime * 0.01f;
	std::memcpy(mVulkanUniformBuffers[mCurrentBufferIndex].mMappedData, &mVulkanUniformData, sizeof(VulkanUniformData));
}

void VulkanRenderer::SubmitFrame()
{
	const VkPipelineStageFlags waitPipelineStage{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	const VkSubmitInfo submitInfo{
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

	const VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &mVkRenderCompleteSemaphores[mCurrentImageIndex],
		.swapchainCount = 1,
		.pSwapchains = &mVulkanSwapChain.mVkSwapchainKHR,
		.pImageIndices = &mCurrentImageIndex
	};

	const VkResult result = vkQueuePresentKHR(mVkQueue, &presentInfo);
	// Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
	if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR) || mEngineProperties->mIsFramebufferResized)
	{
		mEngineProperties->mIsFramebufferResized = false;

		OnResizeWindow();

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			return;
		}
	}
	else
	{
		VK_CHECK_RESULT(result);
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

	const VkApplicationInfo applicationInfo{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = mEngineProperties->mApplicationName.c_str(),
		.pEngineName = mEngineProperties->mEngineName.c_str(),
		.engineVersion = VK_MAKE_VERSION(mEngineProperties->mEngineMajorVersion, mEngineProperties->mEngineMinorVersion, mEngineProperties->mEnginePatchVersion),
		.apiVersion = mEngineProperties->mAPIVersion
	};

	VkInstanceCreateInfo instanceCreateInfo{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &applicationInfo
	};

	if (mEngineProperties->mIsValidationEnabled)
	{
		VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
		VulkanDebug::SetupDebugingMessengerCreateInfo(debugUtilsMessengerCreateInfo);
		debugUtilsMessengerCreateInfo.pNext = instanceCreateInfo.pNext;
		instanceCreateInfo.pNext = &debugUtilsMessengerCreateInfo;
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
		instanceCreateInfo.enabledExtensionCount = static_cast<std::uint32_t>(mInstanceExtensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = mInstanceExtensions.data();

#ifndef NDEBUG
		for (const char* instanceExtension : mInstanceExtensions)
		{
			std::cout << "Enabling instance extension " << instanceExtension << std::endl;
		}
#endif
	}

	if (mEngineProperties->mIsValidationEnabled)
	{
		std::uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);

		std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());

		const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
		bool isValidationLayerPresent = false;
		for (const VkLayerProperties& layer : instanceLayerProperties)
		{
			if (std::strcmp(layer.layerName, validationLayerName) == 0)
			{
				isValidationLayerPresent = true;
				break;
			}
		}
		if (isValidationLayerPresent)
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
	if (mEnabledLayerSettings.size() > 0)
	{
		const VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo{
			.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
			.pNext = instanceCreateInfo.pNext,
			.settingCount = static_cast<std::uint32_t>(mEnabledLayerSettings.size()),
			.pSettings = mEnabledLayerSettings.data(),
		};
		instanceCreateInfo.pNext = &layerSettingsCreateInfo;
	}

	const VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &mVkInstance);
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

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(mVkInstance, &physicalDeviceCount, physicalDevices.data()));

	std::uint32_t selectedDevice = 0;
	VkPhysicalDevice vkPhysicalDevice = physicalDevices[selectedDevice];
	mVulkanDevice = new VulkanDevice();
	mVulkanDevice->CreatePhysicalDevice(vkPhysicalDevice);
	mVulkanDevice->CreateLogicalDevice(mEnabledDeviceExtensions, &mVkPhysicalDevice13Features, true, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
}

void VulkanRenderer::CreatePipelineCache()
{
	const VkPipelineCacheCreateInfo vkPipelineCacheCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
	};
	VK_CHECK_RESULT(vkCreatePipelineCache(mVulkanDevice->mLogicalVkDevice, &vkPipelineCacheCreateInfo, nullptr, &mVkPipelineCache));
}

void VulkanRenderer::PrepareIndirectData()
{
	mDrawIndexedIndirectCommands.clear();

	// Create an indirect command for a node in the scene with a mesh attached to it
	std::uint32_t meshNodeIndex = 0;
	for (const vkglTF::Node* node : mModels.mRockModel->nodes)
	{
		if (node->mMesh)
		{
			// A glTF node may consist of multiple primitives, but for now we only care about the first primitive
			const VkDrawIndexedIndirectCommand drawIndexedIndirectCommand{
				.indexCount = node->mMesh->mPrimitives[0]->indexCount,
				.instanceCount = gRockInstanceCount,
				.firstIndex = node->mMesh->mPrimitives[0]->firstIndex,
				.firstInstance = meshNodeIndex * gRockInstanceCount,
			};
			mDrawIndexedIndirectCommands.push_back(drawIndexedIndirectCommand);

			meshNodeIndex++;
		}
	}

	mIndirectDrawCount = static_cast<std::uint32_t>(mDrawIndexedIndirectCommands.size());

	mIndirectInstanceCount = 0;
	for (const VkDrawIndexedIndirectCommand& indirectCmd : mDrawIndexedIndirectCommands)
	{
		mIndirectInstanceCount += indirectCmd.instanceCount;
	}

	VulkanBuffer stagingBuffer;
	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		mDrawIndexedIndirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand),
		mDrawIndexedIndirectCommands.data()));

	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&mIndirectCommandsBuffer,
		stagingBuffer.mVkDeviceSize));

	mVulkanDevice->CopyBuffer(&stagingBuffer, &mIndirectCommandsBuffer, mVkQueue);

	stagingBuffer.Destroy();
}

void VulkanRenderer::PrepareInstanceData()
{
	std::vector<VulkanInstanceData> instanceData;
	instanceData.resize(mIndirectInstanceCount);

	std::default_random_engine RandomGenerator(std::random_device{}());
	std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);
	std::uniform_int_distribution<std::uint32_t> randomTextureIndex(0, mTextures.mRockTextureArray.mLayerCount);

	// Distribute rocks randomly on two different rings
	for (std::uint32_t i = 0; i < mIndirectInstanceCount / 2; i++)
	{
		glm::vec2 ring0{7.0f, 11.0f};
		glm::vec2 ring1{14.0f, 18.0f};

		// Inner ring
		const float rhoInner = std::sqrt((std::pow(ring0[1], 2.0f) - std::pow(ring0[0], 2.0f)) * uniformDist(RandomGenerator) + std::pow(ring0[0], 2.0f));
		const float thetaInner = static_cast<float>(2.0f * std::numbers::pi * uniformDist(RandomGenerator));
		instanceData[i].mPosition = glm::vec3(rhoInner * std::cos(thetaInner), uniformDist(RandomGenerator) * 0.5f - 0.25f, rhoInner * std::sin(thetaInner));
		instanceData[i].mRotation = glm::vec3(std::numbers::pi * uniformDist(RandomGenerator), std::numbers::pi * uniformDist(RandomGenerator), std::numbers::pi * uniformDist(RandomGenerator));
		instanceData[i].mScale = 1.5f + uniformDist(RandomGenerator) - uniformDist(RandomGenerator);
		instanceData[i].mTextureIndex = randomTextureIndex(RandomGenerator);
		instanceData[i].mScale *= 0.75f;

		// Outer ring
		const float rhoOuter = std::sqrt((std::pow(ring1[1], 2.0f) - std::pow(ring1[0], 2.0f)) * uniformDist(RandomGenerator) + std::pow(ring1[0], 2.0f));
		const float thetaOuter = static_cast<float>(2.0f * std::numbers::pi * uniformDist(RandomGenerator));
		instanceData[i + mIndirectInstanceCount / 2].mPosition = glm::vec3(rhoOuter * std::cos(thetaOuter), uniformDist(RandomGenerator) * 0.5f - 0.25f, rhoOuter * std::sin(thetaOuter));
		instanceData[i + mIndirectInstanceCount / 2].mRotation = glm::vec3(std::numbers::pi * uniformDist(RandomGenerator), std::numbers::pi * uniformDist(RandomGenerator), std::numbers::pi * uniformDist(RandomGenerator));
		instanceData[i + mIndirectInstanceCount / 2].mScale = 1.5f + uniformDist(RandomGenerator) - uniformDist(RandomGenerator);
		instanceData[i + mIndirectInstanceCount / 2].mTextureIndex = randomTextureIndex(RandomGenerator);
		instanceData[i + mIndirectInstanceCount / 2].mScale *= 0.75f;
	}

	VulkanBuffer stagingBuffer;
	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		instanceData.size() * sizeof(VulkanInstanceData),
		instanceData.data()));

	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&mInstanceBuffer,
		stagingBuffer.mVkDeviceSize));

	mVulkanDevice->CopyBuffer(&stagingBuffer, &mInstanceBuffer, mVkQueue);

	stagingBuffer.Destroy();
}

void VulkanRenderer::InitializeSwapchain()
{
	mVulkanSwapChain.InitializeSurface();
}

VkPipelineShaderStageCreateInfo VulkanRenderer::LoadShader(const std::filesystem::path& aPath, VkShaderStageFlagBits aVkShaderStageMask)
{
	const VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = aVkShaderStageMask,
		.module = VulkanTools::LoadShader(aPath, mVulkanDevice->mLogicalVkDevice),
		.pName = "main"
	};

	if (pipelineShaderStageCreateInfo.module == VK_NULL_HANDLE)
	{
		throw std::runtime_error(std::format("Incorrect shader module for shader {}", aPath.generic_string()));
	}

	mVkShaderModules.push_back(pipelineShaderStageCreateInfo.module);
	return pipelineShaderStageCreateInfo;
}

void VulkanRenderer::RenderFrame()
{
	mFrameTimer->StartTimer();
	
	PrepareFrame();
	UpdateUniformBuffers();
	UpdateModelMatrix();
	BuildGraphicsCommandBuffer();
	SubmitFrame();

	mFrameTimer->EndTimer();

	mFrametime = static_cast<float>(mFrameTimer->GetDurationSeconds());

	mFrameCounter++;
	const float fpsTimer = static_cast<float>(Time::GetDurationMilliseconds(mFrameTimer->GetEndTime(), mLastTimestamp));
	if (fpsTimer > mFPSTimerInterval)
	{
		mAverageFPS = static_cast<std::uint32_t>(static_cast<float>(mFrameCounter) * (mFPSTimerInterval / fpsTimer));
		mFrameCounter = 0;
		mLastTimestamp = mFrameTimer->GetEndTime();
	}
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
	const VkCommandPoolCreateInfo vkCommandPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = mVulkanSwapChain.mQueueNodeIndex,
	};
	VK_CHECK_RESULT(vkCreateCommandPool(mVulkanDevice->mLogicalVkDevice, &vkCommandPoolCreateInfo, nullptr, &mVkCommandPoolBuffer));
}

void VulkanRenderer::OnResizeWindow()
{
	if (!mEngineProperties->mIsRendererPrepared)
		return;
	
	mEngineProperties->mIsRendererPrepared = false;

	// Ensure all operations on the device have been finished before destroying resources
	vkDeviceWaitIdle(mVulkanDevice->mLogicalVkDevice);

	// Recreate swap chain
	SetupSwapchain();

	// Recreate the frame buffers
	vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImageView, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkImage, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mVulkanDepthStencil.mVkDeviceMemory, nullptr);
	SetupDepthStencil();

	if ((mFramebufferWidth > 0.0f) && (mFramebufferHeight > 0.0f))
	{
		mImGuiOverlay->Resize(mFramebufferWidth, mFramebufferHeight);
	}

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
		mCamera->UpdateAspectRatio(static_cast<float>(mFramebufferWidth) / static_cast<float>(mFramebufferHeight));
	}

	mEngineProperties->mIsRendererPrepared = true;
}

void VulkanRenderer::SetupSwapchain()
{
	mVulkanSwapChain.CreateSwapchain(mFramebufferWidth, mFramebufferHeight, mEngineProperties->mIsVSyncEnabled);
}

void VulkanRenderer::DrawImGuiOverlay(const VkCommandBuffer aVkCommandBuffer)
{
	const VkViewport viewport{.width = static_cast<float>(mFramebufferWidth), .height = static_cast<float>(mFramebufferHeight), .minDepth = 0.0f, .maxDepth = 1.0f};
	const VkRect2D scissor{.extent = {.width = mFramebufferWidth, .height = mFramebufferHeight }};
	vkCmdSetViewport(aVkCommandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(aVkCommandBuffer, 0, 1, &scissor);
	mImGuiOverlay->Draw(aVkCommandBuffer, mCurrentBufferIndex);
}

void VulkanRenderer::UpdateUIOverlay()
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(static_cast<float>(mFramebufferWidth), static_cast<float>(mFramebufferHeight));
	io.DeltaTime = mFrametime;

	const Input::InputManager& inputManager = Input::InputManager::GetInstance();
	io.MousePos = ImVec2(inputManager.GetMousePosition().x, inputManager.GetMousePosition().y);
	io.MouseDown[0] = inputManager.GetIsMouseButtonDown(Input::MouseButtons::Left) && mImGuiOverlay->IsVisible();
	io.MouseDown[1] = inputManager.GetIsMouseButtonDown(Input::MouseButtons::Right) && mImGuiOverlay->IsVisible();
	io.MouseDown[2] = inputManager.GetIsMouseButtonDown(Input::MouseButtons::Middle) && mImGuiOverlay->IsVisible();

	ImGui::NewFrame();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::SetNextWindowPos(ImVec2(10.0f * mImGuiOverlay->GetScale(), 10.0f * mImGuiOverlay->GetScale()));
	ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);
	ImGui::Begin(mEngineProperties->mApplicationName.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
	ImGui::TextUnformatted(mVulkanDevice->mVkPhysicalDeviceProperties.deviceName);
	ImGui::Text("%i/%i", mFramebufferWidth, mFramebufferHeight);
	ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / mAverageFPS), mAverageFPS);
	mImGuiOverlay->Vec2Text("Cursor position", inputManager.GetMousePosition());

	ImGui::PushItemWidth(110.0f * mImGuiOverlay->GetScale());

	ImGui::NewLine();

	OnUpdateUIOverlay();

	ImGui::PopItemWidth();
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::Render();

	mImGuiOverlay->Update(mCurrentBufferIndex);
}

void VulkanRenderer::OnUpdateUIOverlay()
{
	if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("samplerAnisotropy is %s", mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy ? "enabled" : "disabled");
		ImGui::Text("multiDrawIndirect is %s", mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.multiDrawIndirect ? "enabled" : "disabled");
		ImGui::Text("VSync is %s", mEngineProperties->mIsVSyncEnabled ? "enabled" : "disabled");
		ImGui::Text("Validation Layers is %s", mEngineProperties->mIsValidationEnabled ? "enabled" : "disabled");
	}

	ImGui::NewLine();

	if (ImGui::CollapsingHeader("Scene Details", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Rock instances: %d", mIndirectInstanceCount);

		ImGui::NewLine();

		const glm::vec3& cameraPosition = mCamera->GetPosition();
		mImGuiOverlay->Vec3Text("Camera position", cameraPosition);

		const glm::vec3& cameraRotaiton = mCamera->GetRotation();
		mImGuiOverlay->Vec3Text("Camera rotation", cameraRotaiton);

		const glm::vec4& cameraViewPosition = mCamera->GetViewPosition();
		mImGuiOverlay->Vec4Text("Camera view position", cameraViewPosition);

		ImGui::NewLine();

		mImGuiOverlay->Mat4Text("Voyager", mVoyagerModelMatrix);

		ImGui::NewLine();

		mImGuiOverlay->Mat4Text("Planet", mPlanetModelMatrix);
	}
}
