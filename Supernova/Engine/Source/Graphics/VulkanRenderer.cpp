#include "VulkanRenderer.hpp"

#include "Camera.hpp"
#include "Core/Constants.hpp"
#include "Core/Types.hpp"
#include "EngineProperties.hpp"
#include "FileLoader.hpp"
#include "ImGuiOverlay.hpp"
#include "Input/InputManager.hpp"
#include "Math/Functions.hpp"
#include "Math/Types.hpp"
#include "ModelManager.hpp"
#include "Profiler/SimpleProfiler.hpp"
#include "Profiler/SimpleProfilerImGui.hpp"
#include "TextureManager.hpp"
#include "Time.hpp"
#include "Timer.hpp"
#include "VulkanDebug.hpp"
#include "VulkanDevice.hpp"
#include "VulkanInitializers.hpp"
#include "VulkanTools.hpp"
#include "VulkanTypes.hpp"
#include "Window.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <format>
#include <imgui.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

VulkanRenderer::VulkanRenderer(EngineProperties* aEngineProperties,
	const std::shared_ptr<Window>& aWindow)
	: mEngineProperties{aEngineProperties}
	, mWindow{aWindow}
	, mFramebufferWidth{0}
	, mFramebufferHeight{0}
	, mFrametime{1.0f}
	, mVulkanDevice{nullptr}
	, mImGuiOverlay{nullptr}
	, mCamera{nullptr}
	, mFrameTimer{nullptr}
	, mTextureManager{nullptr}
	, mModelManager{nullptr}
	, mFrameCounter{0}
	, mAverageFPS{0}
	, mFPSTimerInterval{1000.0f}
	, mInstance{VK_NULL_HANDLE}
	, mVkDepthFormat{VK_FORMAT_UNDEFINED}
	, mDescriptorPool{VK_NULL_HANDLE}
	, mPipelineCache{VK_NULL_HANDLE}
	, mBufferIndexCount{0}
	, mCurrentImageIndex{0}
	, mCurrentBufferIndex{0}
	, mIndirectDrawCount{0}
	, mPhysicalDevice13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES}
	, mVoyagerModelMatrix{1.0f}
	, mPlanetModelMatrix{1.0f}
	, mClearColor{0.25f, 0.25f, 0.25f, 1.0f}
	, mLightPosition{0.5f, 0.0f, 35.0f, 1.0f}
	, mShouldShowEditorInfo{true}
	, mShouldShowProfiler{false}
	, mShouldShowModelInspector{false}
	, mShouldFreezeFrustum{false}
#ifdef _DEBUG
	, mShouldDrawWireframe{false}
#endif
{
	mFrameTimer = std::make_unique<Time::Timer>();

	mTextureManager = std::make_shared<TextureManager>();
	mModelManager = std::make_unique<ModelManager>(mTextureManager);
	
	mEngineProperties->mAPIVersion = VK_API_VERSION_1_4;
	mEngineProperties->mIsValidationEnabled = true;
	mEngineProperties->mIsVSyncEnabled = true;

	mFramebufferWidth = mWindow.lock()->GetWindowProperties().mWindowWidth;
	mFramebufferHeight = mWindow.lock()->GetWindowProperties().mWindowHeight;

	mPhysicalDevice13Features.dynamicRendering = VK_TRUE;

	mImGuiOverlay = std::make_unique<ImGuiOverlay>();

	// Setup a default look-at camera
	mCamera = std::make_unique<Camera>();
	mCamera->SetType(CameraType::FirstPerson);
	mCamera->SetPosition(Math::Vector3f(0.5f, 0.0f, -18.5f));
	mCamera->SetRotationSpeed(10.0f);
	mCamera->SetPerspective(60.0f, static_cast<float>(mFramebufferWidth) / static_cast<float>(mFramebufferHeight), 0.1f, 512.0f);

	mVoyagerModelMatrix = Math::Translate(mVoyagerModelMatrix, Math::Vector3f{1.0f, -2.0f, 10.0f});
	mVoyagerModelMatrix = Math::Scale(mVoyagerModelMatrix, Math::Vector3f{0.2f});
}

VulkanRenderer::~VulkanRenderer()
{
	mVulkanSwapChain.CleanUp();

	if (mVulkanDevice->mLogicalVkDevice != VK_NULL_HANDLE)
	{
		if (mDescriptorPool != VK_NULL_HANDLE)
			vkDestroyDescriptorPool(mVulkanDevice->mLogicalVkDevice, mDescriptorPool, nullptr);

		vkFreeCommandBuffers(mVulkanDevice->mLogicalVkDevice, mGraphicsContext.mCommandPool, static_cast<Core::uint32>(mGraphicsContext.mCommandBuffers.size()), mGraphicsContext.mCommandBuffers.data());

		for (VkShaderModule& shaderModule : mShaderModules)
			vkDestroyShaderModule(mVulkanDevice->mLogicalVkDevice, shaderModule, nullptr);

		vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mDepthStencil.mVkImageView, nullptr);
		vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mDepthStencil.mVkImage, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mDepthStencil.mVkDeviceMemory, nullptr);

		vkDestroyPipelineCache(mVulkanDevice->mLogicalVkDevice, mPipelineCache, nullptr);

		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipelines.mPlanet, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipelines.mInstancedSuzanne, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipelines.mVoyager, nullptr);

#ifdef _DEBUG
		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipelines.mPlanetWireframe, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mVkPipelines.mInstancedSuzanneWireframe, nullptr);
#endif

		vkDestroyPipelineLayout(mVulkanDevice->mLogicalVkDevice, mGraphicsContext.mPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, mGraphicsContext.mDescriptorSetLayout, nullptr);
		vkDestroyCommandPool(mVulkanDevice->mLogicalVkDevice, mGraphicsContext.mCommandPool, nullptr);

		mInstanceBuffer.Destroy();

		for (Buffer& buffer : mIndirectDrawCountBuffers)
			buffer.Destroy();

		for (Buffer& buffer : mIndirectCommandsBuffers)
			buffer.Destroy();

		mComputeContext.mLoDBuffers.Destroy();

		vkDestroyPipelineLayout(mVulkanDevice->mLogicalVkDevice, mComputeContext.mPipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, mComputeContext.mDescriptorSetLayout, nullptr);
		vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mComputeContext.mPipeline, nullptr);
		vkDestroyCommandPool(mVulkanDevice->mLogicalVkDevice, mComputeContext.mCommandPool, nullptr);

		for (VkFence& fence : mComputeContext.mFences)
			vkDestroyFence(mVulkanDevice->mLogicalVkDevice, fence, nullptr);

		for (ComputeContext::ComputeSemaphores& semaphore : mComputeContext.mSemaphores)
		{
			vkDestroySemaphore(mVulkanDevice->mLogicalVkDevice, semaphore.mCompleteSemaphore, nullptr);
			vkDestroySemaphore(mVulkanDevice->mLogicalVkDevice, semaphore.mReadySemaphore, nullptr);
		}

		for (VkSemaphore& semaphore : mGraphicsContext.mPresentCompleteSemaphores)
			vkDestroySemaphore(mVulkanDevice->mLogicalVkDevice, semaphore, nullptr);

		for (VkSemaphore& semaphore : mGraphicsContext.mRenderCompleteSemaphores)
			vkDestroySemaphore(mVulkanDevice->mLogicalVkDevice, semaphore, nullptr);

		for (Core::uint32 i = 0; i < gMaxConcurrentFrames; i++)
		{
			vkDestroyFence(mVulkanDevice->mLogicalVkDevice, mGraphicsContext.mFences[i], nullptr);
			vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, mVulkanUniformBuffers[i].mVkBuffer, nullptr);
			vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mVulkanUniformBuffers[i].mVkDeviceMemory, nullptr);
		}

		mTextures.mPlanetTexture.Destroy();
	}

	mImGuiOverlay->FreeResources();

	if (mEngineProperties->mIsValidationEnabled)
		VulkanDebug::DestroyDebugUtilsMessenger(mInstance);

	mModelManager.reset();
	mTextureManager.reset();

	delete mVulkanDevice;

	vkDestroyInstance(mInstance, nullptr);
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
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::UpdateRenderer");

	if (!mWindow.lock()->GetWindowProperties().mIsMinimized)
	{
		if (mEngineProperties->mIsRendererPrepared)
		{
			RenderFrame();
		}

		Input::InputManager& inputManager = Input::InputManager::GetInstance();
		if (!mImGuiOverlay->WantsToCaptureInput())
		{
			mCamera->mKeys.mIsRightDown = inputManager.GetIsKeyDown(Input::Key::Right) || inputManager.GetIsKeyDown(Input::Key::D);
			mCamera->mKeys.mIsUpDown = inputManager.GetIsKeyDown(Input::Key::Up) || inputManager.GetIsKeyDown(Input::Key::W);
			mCamera->mKeys.mIsDownDown = inputManager.GetIsKeyDown(Input::Key::Down) || inputManager.GetIsKeyDown(Input::Key::S);
			mCamera->mKeys.mIsLeftDown = inputManager.GetIsKeyDown(Input::Key::Left) || inputManager.GetIsKeyDown(Input::Key::A);
			mCamera->mKeys.mIsShiftDown = inputManager.GetIsKeyDown(Input::Key::LeftShift);
			mCamera->mKeys.mIsSpaceDown = inputManager.GetIsKeyDown(Input::Key::Spacebar);
			mCamera->mKeys.mIsCtrlDown = inputManager.GetIsKeyDown(Input::Key::LeftControl);
			mCamera->mMouse.mScrollWheelDelta = inputManager.GetScrollOffset().y;
			mCamera->mMouse.mIsLeftDown = inputManager.GetIsMouseButtonDown(Input::MouseButton::Left);
			mCamera->mMouse.mIsMiddleDown = inputManager.GetIsMouseButtonDown(Input::MouseButton::Middle);
			mCamera->mMouse.mDeltaX = inputManager.GetMousePositionDelta().x;
			mCamera->mMouse.mDeltaY = inputManager.GetMousePositionDelta().y;
		}
		else
		{
			mCamera->mKeys.mIsRightDown = false;
			mCamera->mKeys.mIsUpDown = false;
			mCamera->mKeys.mIsDownDown = false;
			mCamera->mKeys.mIsLeftDown = false;
			mCamera->mKeys.mIsShiftDown = false;
			mCamera->mKeys.mIsSpaceDown = false;
			mCamera->mKeys.mIsCtrlDown = false;
			mCamera->mMouse.mScrollWheelDelta = false;
			mCamera->mMouse.mIsLeftDown = false;
			mCamera->mMouse.mIsMiddleDown = false;
			mCamera->mMouse.mDeltaX = 0.0f;
			mCamera->mMouse.mDeltaY = 0.0f;
		}

		inputManager.FlushInput(); // TODO: Fix this bad solution to having frame-based offsets

		mCamera->Update(mFrametime);
	}

	mWindow.lock()->UpdateWindow();
}

void VulkanRenderer::LoadAssets()
{
	mTextureManager->SetContext(mVulkanDevice, mGraphicsContext.mQueue);

	const FileLoadingFlags glTFLoadingFlags = FileLoadingFlags::PreTransformVertices | FileLoadingFlags::PreMultiplyVertexColors | FileLoadingFlags::FlipY;
	const std::filesystem::path voyagerModelPath = "Voyager.gltf";
	mModelIdentifiers.mVoyagerModelIdentifier = mModelManager->LoadModel(FileLoader::GetEngineResourcesPath() / FileLoader::gModelsPath / voyagerModelPath, mVulkanDevice, mGraphicsContext.mQueue, glTFLoadingFlags, 1.0f);

	const std::filesystem::path suzanneModelPath = "Suzanne_lods.gltf";
	mModelIdentifiers.mSuzanneModelIdentifier = mModelManager->LoadModel(FileLoader::GetEngineResourcesPath() / FileLoader::gModelsPath / suzanneModelPath, mVulkanDevice, mGraphicsContext.mQueue, glTFLoadingFlags, 1.0f);

	const std::filesystem::path planetModelPath = "Lavaplanet.gltf";
	mModelIdentifiers.mPlanetModelIdentifier = mModelManager->LoadModel(FileLoader::GetEngineResourcesPath() / FileLoader::gModelsPath / planetModelPath, mVulkanDevice, mGraphicsContext.mQueue, glTFLoadingFlags, 1.0f);

	const std::filesystem::path planetTexturePath = "Lavaplanet_rgba.ktx";
	mTextures.mPlanetTexture = mTextureManager->CreateTexture(FileLoader::GetEngineResourcesPath() / FileLoader::gTexturesPath / planetTexturePath);
}

void VulkanRenderer::CreateSynchronizationPrimitives()
{
	// Wait fences to sync command buffer access
	const VkFenceCreateInfo fenceCreateInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
	for (VkFence& fence : mGraphicsContext.mFences)
	{
		VK_CHECK_RESULT(vkCreateFence(mVulkanDevice->mLogicalVkDevice, &fenceCreateInfo, nullptr, &fence));
	}

	// Used to ensure that image presentation is complete before starting to submit again
	const VkSemaphoreCreateInfo semaphoreCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	for (VkSemaphore& semaphore : mGraphicsContext.mPresentCompleteSemaphores)
	{
		VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalVkDevice, &semaphoreCreateInfo, nullptr, &semaphore));
	}

	// Semaphore used to ensure that all commands submitted have been finished before submitting the image to the queue
	mGraphicsContext.mRenderCompleteSemaphores.resize(mVulkanSwapChain.mVkImages.size());
	for (VkSemaphore& semaphore : mGraphicsContext.mRenderCompleteSemaphores)
	{
		VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalVkDevice, &semaphoreCreateInfo, nullptr, &semaphore));
	}
}

// Command buffers are used to record commands to and are submitted to a queue for execution ("rendering")
void VulkanRenderer::CreateGraphicsCommandBuffers()
{
	// Allocate one command buffer per max. concurrent frame from above pool
	const VkCommandBufferAllocateInfo commandBufferAllocateInfo = VulkanInitializers::CommandBufferAllocateInfo(mGraphicsContext.mCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, gMaxConcurrentFrames);
	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalVkDevice, &commandBufferAllocateInfo, mGraphicsContext.mCommandBuffers.data()));
}

void VulkanRenderer::CreateDescriptorPool()
{
	static constexpr Core::uint32 poolPadding = 2;
	const std::vector<VkDescriptorPoolSize> poolSizes = {
		VulkanInitializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (gMaxConcurrentFrames * 3) + poolPadding),
		VulkanInitializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (gMaxConcurrentFrames * 2) + poolPadding),
		VulkanInitializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (gMaxConcurrentFrames * 4) + poolPadding)
	};
	const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = VulkanInitializers::DescriptorPoolCreateInfo(poolSizes, gMaxConcurrentFrames * 4);
	VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalVkDevice, &descriptorPoolCreateInfo, nullptr, &mDescriptorPool));
}

void VulkanRenderer::CreateGraphicsDescriptorSetLayout()
{
	const std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0 : Vertex shader uniform buffer
		VulkanInitializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		// Binding 1 : Fragment shader combined sampler
		VulkanInitializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
	};
	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = VulkanInitializers::DescriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, &descriptorSetLayoutCreateInfo, nullptr, &mGraphicsContext.mDescriptorSetLayout));
}

void VulkanRenderer::CreateGraphicsDescriptorSets()
{
	// Sets per frame, just like the buffers themselves
	const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = VulkanInitializers::DescriptorSetAllocateInfo(mDescriptorPool, &mGraphicsContext.mDescriptorSetLayout, 1);
	for (Core::size i = 0; i < mVulkanUniformBuffers.size(); i++)
	{
		// Instanced models
		// Binding 0 : Vertex shader uniform buffer
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocateInfo, &mDescriptorSets[i].mSuzanneModel));
		const std::vector<VkWriteDescriptorSet> instancedWriteDescriptorSets = {
			VulkanInitializers::WriteDescriptorSet(mDescriptorSets[i].mSuzanneModel, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mVulkanUniformBuffers[i].mVkDescriptorBufferInfo),
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<Core::uint32>(instancedWriteDescriptorSets.size()), instancedWriteDescriptorSets.data(), 0, nullptr);

		// Static planet
		//	Binding 0 : Vertex shader uniform buffer
		//	Binding 1 : Color map
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocateInfo, &mDescriptorSets[i].mStaticPlanet));
		const std::vector<VkWriteDescriptorSet> staticPlanetWriteDescriptorSets = {
			VulkanInitializers::WriteDescriptorSet(mDescriptorSets[i].mStaticPlanet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mVulkanUniformBuffers[i].mVkDescriptorBufferInfo),
			VulkanInitializers::WriteDescriptorSet(mDescriptorSets[i].mStaticPlanet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &mTextures.mPlanetTexture.mDescriptorImageInfo),
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<Core::uint32>(staticPlanetWriteDescriptorSets.size()), staticPlanetWriteDescriptorSets.data(), 0, nullptr);

		// Static voyager
		//	Binding 0 : Vertex shader uniform buffer
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocateInfo, &mDescriptorSets[i].mStaticVoyager));
		const std::vector<VkWriteDescriptorSet> staticVoyagerWriteDescriptorSets = {
			VulkanInitializers::WriteDescriptorSet(mDescriptorSets[i].mStaticVoyager, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mVulkanUniformBuffers[i].mVkDescriptorBufferInfo),
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<Core::uint32>(staticVoyagerWriteDescriptorSets.size()), staticVoyagerWriteDescriptorSets.data(), 0, nullptr);
	}
}

void VulkanRenderer::SetupDepthStencil()
{
	// Create an optimal tiled image used as the depth stencil attachment
	const VkImageCreateInfo imageCreateInfo{
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
	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &mDepthStencil.mVkImage));

	// Allocate memory for the image (device local) and bind it to our image
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalVkDevice, mDepthStencil.mVkImage, &memoryRequirements);

	const VkMemoryAllocateInfo memoryAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &memoryAllocateInfo, nullptr, &mDepthStencil.mVkDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalVkDevice, mDepthStencil.mVkImage, mDepthStencil.mVkDeviceMemory, 0));

	// Create a view for the depth stencil image
	// Images aren't directly accessed in Vulkan, but rather through views described by a subresource range
	// This allows for multiple views of one image with differing ranges (e.g. for different layers)
	VkImageViewCreateInfo imageViewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = mDepthStencil.mVkImage,
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
		imageViewCreateInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalVkDevice, &imageViewCreateInfo, nullptr, &mDepthStencil.mVkImageView));
}

void VulkanRenderer::CreateGraphicsPipelines()
{
	// Layout
	// Uses set 0 for passing vertex shader ubo and set 1 for fragment shader images (taken from glTF model)
	const std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
		mGraphicsContext.mDescriptorSetLayout,
		vkglTF::gDescriptorSetLayoutImage,
	};
	
	const VkPushConstantRange pushConstantRange{
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(PushConstant)
	};

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VulkanInitializers::PipelineLayoutCreateInfo(descriptorSetLayouts.data(), 2);
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalVkDevice, &pipelineLayoutCreateInfo, nullptr, &mGraphicsContext.mPipelineLayout));

	// Pipeline
	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = VulkanInitializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationState = VulkanInitializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	const VkPipelineColorBlendAttachmentState blendAttachmentState = VulkanInitializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE);
	const VkPipelineColorBlendStateCreateInfo colorBlendState = VulkanInitializers::PipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = VulkanInitializers::PipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	const VkPipelineViewportStateCreateInfo viewportState = VulkanInitializers::PipelineViewportStateCreateInfo(1, 1, 0);
	const VkPipelineMultisampleStateCreateInfo multisampleState = VulkanInitializers::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	const VkPipelineDynamicStateCreateInfo dynamicState = VulkanInitializers::PipelineDynamicStateCreateInfo(dynamicStateEnables);
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

	// We no longer need to set a renderpass for the pipeline create info
	VkGraphicsPipelineCreateInfo pipelineCI = VulkanInitializers::PipelineCreateInfo();
	pipelineCI.layout = mGraphicsContext.mPipelineLayout;
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = static_cast<Core::uint32>(shaderStages.size());
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
		VulkanInitializers::VertexInputBindingDescription(0, sizeof(vkglTF::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
		// Binding point 1: Instanced data at per-instance rate
		VulkanInitializers::VertexInputBindingDescription(1, sizeof(InstanceData), VK_VERTEX_INPUT_RATE_INSTANCE),
	};

	const std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
		// Per-vertex attributes
		// These are advanced for each vertex fetched by the vertex shader
		VulkanInitializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, mPosition)), // Location 0: Position
		VulkanInitializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, mNormal)), // Location 1: Normal
		VulkanInitializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, mColor)), // Location 3: Color
		// Per-Instance attributes
		// These are advanced for each instance rendered
		VulkanInitializers::VertexInputAttributeDescription(1, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, mPosition)), // Location 4: Position
		VulkanInitializers::VertexInputAttributeDescription(1, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, mScale)), // Location 5: Scale
	};

	const std::vector<VkVertexInputAttributeDescription> texturedAttributeDescriptions = {
		// Per-vertex attributes
		// These are advanced for each vertex fetched by the vertex shader
		VulkanInitializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, mPosition)), // Location 0: Position
		VulkanInitializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, mNormal)), // Location 1: Normal
		VulkanInitializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(vkglTF::Vertex, mUV)), // Location 2: Texture coordinates
		VulkanInitializers::VertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkglTF::Vertex, mColor)), // Location 3: Color
		// Per-Instance attributes
		// These are advanced for each instance rendered
		VulkanInitializers::VertexInputAttributeDescription(1, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, mPosition)), // Location 4: Position
		VulkanInitializers::VertexInputAttributeDescription(1, 5, VK_FORMAT_R32G32B32_SFLOAT, offsetof(InstanceData, mScale)), // Location 5: Scale
	};

	VkPipelineVertexInputStateCreateInfo inputState = VulkanInitializers::PipelineVertexInputStateCreateInfo();
	inputState.pVertexBindingDescriptions = bindingDescriptions.data();
	inputState.pVertexAttributeDescriptions = texturedAttributeDescriptions.data();

	pipelineCI.pVertexInputState = &inputState;

	const std::filesystem::path voyagerVertexShaderPath = "DynamicRendering/Texture_vert.spv";
	const std::filesystem::path voyagerFragmentShaderPath = "DynamicRendering/Texture_frag.spv";
	shaderStages[0] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / voyagerVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / voyagerFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputState.vertexBindingDescriptionCount = 1;
	inputState.vertexAttributeDescriptionCount = 3;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mPipelineCache, 1, &pipelineCI, nullptr, &mVkPipelines.mVoyager));

	const std::filesystem::path planetVertexShaderPath = "Instancing/Planet_vert.spv";
	const std::filesystem::path planetFragmentShaderPath = "Instancing/Planet_frag.spv";
	shaderStages[0] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / planetVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / planetFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputState.vertexBindingDescriptionCount = 1;
	inputState.vertexAttributeDescriptionCount = 4;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mPipelineCache, 1, &pipelineCI, nullptr, &mVkPipelines.mPlanet));

#ifdef _DEBUG
	if (mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.fillModeNonSolid)
	{
		rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mPipelineCache, 1, &pipelineCI, nullptr, &mVkPipelines.mPlanetWireframe));

		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	}
#endif

	const std::filesystem::path suzanneVertexShaderPath = "ComputeCull/Indirectdraw_vert.spv";
	const std::filesystem::path suzanneFragmentShaderPath = "ComputeCull/Indirectdraw_frag.spv";
	shaderStages[0] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / suzanneVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / suzanneFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
	inputState.pVertexAttributeDescriptions = attributeDescriptions.data();
	inputState.vertexBindingDescriptionCount = static_cast<Core::uint32>(bindingDescriptions.size());
	inputState.vertexAttributeDescriptionCount = static_cast<Core::uint32>(attributeDescriptions.size());
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mPipelineCache, 1, &pipelineCI, nullptr, &mVkPipelines.mInstancedSuzanne));

#ifdef _DEBUG
	if (mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.fillModeNonSolid)
	{
		rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, mPipelineCache, 1, &pipelineCI, nullptr, &mVkPipelines.mInstancedSuzanneWireframe));

		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	}
#endif
}

void VulkanRenderer::CreateComputeDescriptorSetLayout()
{
	vkGetDeviceQueue(mVulkanDevice->mLogicalVkDevice, mVulkanDevice->mQueueFamilyIndices.mCompute, 0, &mComputeContext.mQueue);

	const std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: Instance input data buffer
		VulkanInitializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		// Binding 1: Indirect draw command output buffer (input)
		VulkanInitializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		// Binding 2: Uniform buffer with global matrices (input)
		VulkanInitializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		// Binding 3: Indirect draw stats (output)
		VulkanInitializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
		// Binding 4: LOD info (input)
		VulkanInitializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
	};

	const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = VulkanInitializers::DescriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, &descriptorSetLayoutCreateInfo, nullptr, &mComputeContext.mDescriptorSetLayout));
}

void VulkanRenderer::CreateComputeDescriptorSets()
{
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VulkanInitializers::PipelineLayoutCreateInfo(&mComputeContext.mDescriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalVkDevice, &pipelineLayoutCreateInfo, nullptr, &mComputeContext.mPipelineLayout));

	for (Core::size i = 0; i < mVulkanUniformBuffers.size(); i++)
	{
		VkDescriptorSetAllocateInfo allocInfo = VulkanInitializers::DescriptorSetAllocateInfo(mDescriptorPool, &mComputeContext.mDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &allocInfo, &mComputeContext.mDescriptorSets[i]));
		const std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
			// Binding 0: Instance input data buffer
			VulkanInitializers::WriteDescriptorSet(mComputeContext.mDescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &mInstanceBuffer.mVkDescriptorBufferInfo),
			// Binding 1: Indirect draw command output buffer
			VulkanInitializers::WriteDescriptorSet(mComputeContext.mDescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &mIndirectCommandsBuffers[i].mVkDescriptorBufferInfo),
			// Binding 2: Uniform buffer with global matrices
			VulkanInitializers::WriteDescriptorSet(mComputeContext.mDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &mVulkanUniformBuffers[i].mVkDescriptorBufferInfo),
			// Binding 3: Atomic counter (written in shader)
			VulkanInitializers::WriteDescriptorSet(mComputeContext.mDescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &mIndirectDrawCountBuffers[i].mVkDescriptorBufferInfo),
			// Binding 4: LOD info
			VulkanInitializers::WriteDescriptorSet(mComputeContext.mDescriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &mComputeContext.mLoDBuffers.mVkDescriptorBufferInfo)
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<Core::uint32>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, nullptr);
	}
}

void VulkanRenderer::CreateComputePipelines()
{
	VkComputePipelineCreateInfo computePipelineCreateInfo = VulkanInitializers::ComputePipelineCreateInfo(mComputeContext.mPipelineLayout, 0);
	const std::filesystem::path computeShaderPath = "ComputeCull/Indirectdraw_comp.spv";
	computePipelineCreateInfo.stage = LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / computeShaderPath, VK_SHADER_STAGE_COMPUTE_BIT);

	// Use specialization constants to pass max. level of detail (determined by no. of meshes)
	const VkSpecializationMapEntry specializationEntry =
	{
		.constantID = 0,
		.offset = 0,
		.size = sizeof(Core::uint32)
	};

	const Core::uint32 specializationData = static_cast<Core::uint32>(mModelManager->GetModel(mModelIdentifiers.mSuzanneModelIdentifier)->nodes.size()) - 1;

	const VkSpecializationInfo specializationInfo =
	{
		.mapEntryCount = 1,
		.pMapEntries = &specializationEntry,
		.dataSize = sizeof(specializationData),
		.pData = &specializationData
	};
	computePipelineCreateInfo.stage.pSpecializationInfo = &specializationInfo;

	VK_CHECK_RESULT(vkCreateComputePipelines(mVulkanDevice->mLogicalVkDevice, mPipelineCache, 1, &computePipelineCreateInfo, nullptr, &mComputeContext.mPipeline));

	// Separate command pool as queue family for compute may be different than graphics
	const VkCommandPoolCreateInfo commandPoolCreateInfo =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = mVulkanDevice->mQueueFamilyIndices.mCompute
	};
	VK_CHECK_RESULT(vkCreateCommandPool(mVulkanDevice->mLogicalVkDevice, &commandPoolCreateInfo, nullptr, &mComputeContext.mCommandPool));

	// Create command buffers for compute operations
	for (VkCommandBuffer& commandBuffer : mComputeContext.mCommandBuffers)
	{
		commandBuffer = mVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, mComputeContext.mCommandPool);
	}

	// Fences to check for command buffer completion
	for (VkFence& fence : mComputeContext.mFences)
	{
		const VkFenceCreateInfo fenceCreateInfo = VulkanInitializers::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
		VK_CHECK_RESULT(vkCreateFence(mVulkanDevice->mLogicalVkDevice, &fenceCreateInfo, nullptr, &fence));
	}

	// Semaphores to order compute and graphics submissions
	for (ComputeContext::ComputeSemaphores& semaphore : mComputeContext.mSemaphores)
	{
		const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
		VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalVkDevice, &semaphoreInfo, nullptr, &semaphore.mCompleteSemaphore));
		VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalVkDevice, &semaphoreInfo, nullptr, &semaphore.mReadySemaphore));
	}

	// Signal first used ready semaphore
	const VkSubmitInfo computeSubmitInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &mComputeContext.mSemaphores[gMaxConcurrentFrames - 1].mReadySemaphore
	};
	VK_CHECK_RESULT(vkQueueSubmit(mComputeContext.mQueue, 1, &computeSubmitInfo, VK_NULL_HANDLE));
}

void VulkanRenderer::CreateUniformBuffers()
{
	for (Buffer& buffer : mVulkanUniformBuffers)
	{
		VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer, sizeof(UniformBufferData), &mUniformBufferData));
		VK_CHECK_RESULT(buffer.Map());
	}
}

void VulkanRenderer::CreateUIOverlay()
{
	const std::filesystem::path UIVertexShaderPath = "Core/UIOverlay_vert.spv";
	const std::filesystem::path UIFragmentShaderPath = "Core/UIOverlay_frag.spv";
	mImGuiOverlay->SetMaxConcurrentFrames(gMaxConcurrentFrames);
	mImGuiOverlay->SetVulkanDevice(mVulkanDevice);
	mImGuiOverlay->SetVkQueue(mGraphicsContext.mQueue);
	mImGuiOverlay->SetScale(mWindow.lock()->GetContentScaleForMonitor());
	mImGuiOverlay->AddShader(LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / UIVertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT));
	mImGuiOverlay->AddShader(LoadShader(FileLoader::GetEngineResourcesPath() / FileLoader::gShadersPath / UIFragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT));
	mImGuiOverlay->PrepareResources();
	mImGuiOverlay->PreparePipeline(mPipelineCache, mVulkanSwapChain.mColorVkFormat, mVkDepthFormat);
}

void VulkanRenderer::PrepareVulkanResources()
{
	InitializeSwapchain();
	CreateGraphicsCommandPool();
	SetupSwapchain();
	CreateGraphicsCommandBuffers();
	CreateSynchronizationPrimitives();
	SetupDepthStencil();
	CreatePipelineCache();

	CreateUIOverlay();

	LoadAssets();
	
	PrepareIndirectData();
	PrepareInstanceData();
	CreateUniformBuffers();
	CreateDescriptorPool();
	CreateGraphicsDescriptorSetLayout();
	CreateGraphicsDescriptorSets();
	CreateGraphicsPipelines();
	CreateComputeDescriptorSetLayout();
	CreateComputeDescriptorSets();
	CreateComputePipelines();

	mEngineProperties->mIsRendererPrepared = true;
}

void VulkanRenderer::PrepareFrameGraphics()
{
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::PrepareFrameGraphics");

	// Use a fence to wait until the command buffer has finished execution before using it again
	VK_CHECK_RESULT(vkWaitForFences(mVulkanDevice->mLogicalVkDevice, 1, &mGraphicsContext.mFences[mCurrentBufferIndex], VK_TRUE, Core::uint64_max));
	VK_CHECK_RESULT(vkResetFences(mVulkanDevice->mLogicalVkDevice, 1, &mGraphicsContext.mFences[mCurrentBufferIndex]));

	UpdateUIOverlay();

	const VkResult result = mVulkanSwapChain.AcquireNextImage(mGraphicsContext.mPresentCompleteSemaphores[mCurrentBufferIndex], mCurrentImageIndex);
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
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::BuildGraphicsCommandBuffer");

	VkCommandBuffer commandBuffer = mGraphicsContext.mCommandBuffers[mCurrentBufferIndex];

	const VkCommandBufferBeginInfo commandBufferBeginInfo = VulkanInitializers::CommandBufferBeginInfo();
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
		mDepthStencil.mVkImage,
		0,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VkImageSubresourceRange{VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1});

	if (mVulkanDevice->mQueueFamilyIndices.mGraphics != mVulkanDevice->mQueueFamilyIndices.mCompute)
	{
		const VkBufferMemoryBarrier bufferMemoryBarrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			nullptr,
			0,
			VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
			mVulkanDevice->mQueueFamilyIndices.mCompute,
			mVulkanDevice->mQueueFamilyIndices.mGraphics,
			mIndirectCommandsBuffers[mCurrentBufferIndex].mVkBuffer,
			0,
			mIndirectCommandsBuffers[mCurrentBufferIndex].mVkDescriptorBufferInfo.range
		};
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			0,
			0,
			nullptr,
			1,
			&bufferMemoryBarrier,
			0,
			nullptr);
	}

	// New structures are used to define the attachments used in dynamic rendering
	const VkRenderingAttachmentInfoKHR colorAttachmentInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
		.imageView = mVulkanSwapChain.mVkImageViews[mCurrentImageIndex],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = { .color = {mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a} }
	};

	// A single depth stencil attachment info can be used, but they can also be specified separately.
	// When both are specified separately, the only requirement is that the image view is identical.			
	const VkRenderingAttachmentInfoKHR depthStencilAttachmentInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
		.imageView = mDepthStencil.mVkImageView,
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

	const VkViewport viewport = VulkanInitializers::Viewport(static_cast<float>(mFramebufferWidth), static_cast<float>(mFramebufferHeight), 0.0f, 1.0f);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	const VkRect2D scissor = VulkanInitializers::Rect2D(mFramebufferWidth, mFramebufferHeight, 0, 0);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// Draw non-instanced static models
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsContext.mPipelineLayout, 0, 1, &mDescriptorSets[mCurrentBufferIndex].mStaticPlanet, 0, nullptr);

#ifdef _DEBUG
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mShouldDrawWireframe ? mVkPipelines.mPlanetWireframe : mVkPipelines.mPlanet);
#else
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelines.mPlanet);
#endif

	mPushConstant.mModelMatrix = mPlanetModelMatrix;
	vkCmdPushConstants(commandBuffer, mGraphicsContext.mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &mPushConstant);

	DrawModel(mModelManager->GetModel(mModelIdentifiers.mPlanetModelIdentifier), commandBuffer);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsContext.mPipelineLayout, 0, 1, &mDescriptorSets[mCurrentBufferIndex].mStaticVoyager, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelines.mVoyager);

	mPushConstant.mModelMatrix = mVoyagerModelMatrix;
	vkCmdPushConstants(commandBuffer, mGraphicsContext.mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &mPushConstant);

	DrawModel(mModelManager->GetModel(mModelIdentifiers.mVoyagerModelIdentifier), commandBuffer, RenderFlags::BindImages, mGraphicsContext.mPipelineLayout);

	// Draw instanced multi draw models
	const VkDeviceSize offsets[1] = {0};
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsContext.mPipelineLayout, 0, 1, &mDescriptorSets[mCurrentBufferIndex].mSuzanneModel, 0, nullptr);

#ifdef _DEBUG
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mShouldDrawWireframe ? mVkPipelines.mInstancedSuzanneWireframe : mVkPipelines.mInstancedSuzanne);
#else
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mVkPipelines.mInstancedSuzanne);
#endif

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mModelManager->GetModel(mModelIdentifiers.mSuzanneModelIdentifier)->vertices.mBuffer, offsets);
	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &mInstanceBuffer.mVkBuffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, mModelManager->GetModel(mModelIdentifiers.mSuzanneModelIdentifier)->indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
	
	// One draw call for an arbitrary number of objects
	if (mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.multiDrawIndirect)
	{
		// Index offsets and instance count are taken from the indirect buffer
		vkCmdDrawIndexedIndirect(commandBuffer, mIndirectCommandsBuffers[mCurrentBufferIndex].mVkBuffer, 0, static_cast<Core::uint32>(mIndirectCommands.size()), sizeof(VkDrawIndexedIndirectCommand));
	}
	else
	{
		// Issue separate draw commands
		for (Core::size j = 0; j < mIndirectCommands.size(); j++)
		{
			vkCmdDrawIndexedIndirect(commandBuffer, mIndirectCommandsBuffers[mCurrentBufferIndex].mVkBuffer, j * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
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

	if (mVulkanDevice->mQueueFamilyIndices.mGraphics != mVulkanDevice->mQueueFamilyIndices.mCompute)
	{
		const VkBufferMemoryBarrier bufferMemoryBarrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			nullptr,
			VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
			0,
			mVulkanDevice->mQueueFamilyIndices.mGraphics,
			mVulkanDevice->mQueueFamilyIndices.mCompute,
			mIndirectCommandsBuffers[mCurrentBufferIndex].mVkBuffer,
			0,
			mIndirectCommandsBuffers[mCurrentBufferIndex].mVkDescriptorBufferInfo.range
		};
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0,
			0,
			nullptr,
			1,
			&bufferMemoryBarrier,
			0,
			nullptr);
	}

	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
}

void VulkanRenderer::PrepareFrameCompute()
{
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::PrepareFrameCompute");

	VK_CHECK_RESULT(vkWaitForFences(mVulkanDevice->mLogicalVkDevice, 1, &mComputeContext.mFences[mCurrentBufferIndex], VK_TRUE, Core::uint64_max));
	VK_CHECK_RESULT(vkResetFences(mVulkanDevice->mLogicalVkDevice, 1, &mComputeContext.mFences[mCurrentBufferIndex]));

	// Get draw count from compute
	std::memcpy(&mIndrectDrawInfo, mIndirectDrawCountBuffers[mCurrentBufferIndex].mMappedData, sizeof(mIndrectDrawInfo));
}

void VulkanRenderer::BuildComputeCommandBuffer()
{
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::BuildComputeCommandBuffer");

	VkCommandBuffer commandBuffer = mComputeContext.mCommandBuffers[mCurrentBufferIndex];

	const VkCommandBufferBeginInfo commandBufferBeginInfo = VulkanInitializers::CommandBufferBeginInfo();
	VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

	// Acquire barrier
	// Add memory barrier to ensure that the indirect commands have been consumed before the compute shader updates them
	if (mVulkanDevice->mQueueFamilyIndices.mGraphics != mVulkanDevice->mQueueFamilyIndices.mCompute)
	{
		const VkBufferMemoryBarrier bufferMemoryBarrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			nullptr,
			0,
			VK_ACCESS_SHADER_WRITE_BIT,
			mVulkanDevice->mQueueFamilyIndices.mGraphics,
			mVulkanDevice->mQueueFamilyIndices.mCompute,
			mIndirectCommandsBuffers[mCurrentBufferIndex].mVkBuffer,
			0,
			mIndirectCommandsBuffers[mCurrentBufferIndex].mVkDescriptorBufferInfo.range
		};
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0,
			nullptr,
			1,
			&bufferMemoryBarrier,
			0,
			nullptr);
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mComputeContext.mPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mComputeContext.mPipelineLayout, 0, 1, &mComputeContext.mDescriptorSets[mCurrentBufferIndex], 0, nullptr);

	// Clear the buffer that the compute shader pass will write statistics and draw calls to
	vkCmdFillBuffer(commandBuffer, mIndirectDrawCountBuffers[mCurrentBufferIndex].mVkBuffer, 0, mIndirectDrawCountBuffers[mCurrentBufferIndex].mVkDescriptorBufferInfo.range, 0);

	// This barrier ensures that the fill command is finished before the compute shader can start writing to the buffer
	const VkMemoryBarrier memoryBarrier =
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
	};
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		1,
		&memoryBarrier,
		0,
		nullptr,
		0,
		nullptr);

	// Dispatch the compute job
	// The compute shader will do the frustum culling and adjust the indirect draw calls depending on object visibility.
	// It also determines the lod to use depending on distance to the viewer.
	vkCmdDispatch(commandBuffer, mIndirectDrawCount / 16, 1, 1);

	// Release barrier
	// Add memory barrier to ensure that the compute shader has finished writing the indirect command buffer before it's consumed
	if (mVulkanDevice->mQueueFamilyIndices.mGraphics != mVulkanDevice->mQueueFamilyIndices.mCompute)
	{
		const VkBufferMemoryBarrier bufferMemoryBarrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			nullptr,
			VK_ACCESS_SHADER_WRITE_BIT,
			0,
			mVulkanDevice->mQueueFamilyIndices.mCompute,
			mVulkanDevice->mQueueFamilyIndices.mGraphics,
			mIndirectCommandsBuffers[mCurrentBufferIndex].mVkBuffer,
			0,
			mIndirectCommandsBuffers[mCurrentBufferIndex].mVkDescriptorBufferInfo.range
		};
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0,
			0,
			nullptr,
			1,
			&bufferMemoryBarrier,
			0,
			nullptr);
	}

	vkEndCommandBuffer(commandBuffer);
}

void VulkanRenderer::UpdateModelMatrix()
{
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::UpdateModelMatrix");

	const Math::Vector3f pivotPoint = Math::Vector3f{20.0f, 0.0f, 80.0f};
	mVoyagerModelMatrix = Math::Translate(mVoyagerModelMatrix, -pivotPoint);

	static constexpr float angle = Math::ToRadians(-5.0f);
	const Math::Vector3f rotationAxis = Math::Vector3f{0.0f, 1.0f, 0.0f};
	mVoyagerModelMatrix = Math::Rotate(mVoyagerModelMatrix, angle * mFrametime, rotationAxis);

	mVoyagerModelMatrix = Math::Translate(mVoyagerModelMatrix, pivotPoint);
}

void VulkanRenderer::UpdateUniformBuffers()
{
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::UpdateUniformBuffers");

	mUniformBufferData.mProjectionMatrix = mCamera->mMatrices.mPerspective;
	mUniformBufferData.mViewMatrix = mCamera->mMatrices.mView;
	mUniformBufferData.mLightPosition = mLightPosition;

	if (!mShouldFreezeFrustum)
	{
		mUniformBufferData.mViewPosition = mCamera->GetViewPosition();
		mViewFrustum.UpdateFrustum(mUniformBufferData.mProjectionMatrix * mUniformBufferData.mViewMatrix);
		std::memcpy(mUniformBufferData.mFrustumPlanes, mViewFrustum.mPlanes.data(), sizeof(Math::Vector4f) * 6);
	}

	std::memcpy(mVulkanUniformBuffers[mCurrentBufferIndex].mMappedData, &mUniformBufferData, sizeof(UniformBufferData));
}

void VulkanRenderer::SubmitFrameGraphics()
{
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::SubmitFrameGraphics");

	const VkPipelineStageFlags waitPipelineStageMask[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT };
	const VkSemaphore waitSemaphores[2] = {mGraphicsContext.mPresentCompleteSemaphores[mCurrentBufferIndex], mComputeContext.mSemaphores[mCurrentBufferIndex].mCompleteSemaphore};
	const VkSemaphore signalSemaphores[2] = {mGraphicsContext.mRenderCompleteSemaphores[mCurrentImageIndex], mComputeContext.mSemaphores[mCurrentBufferIndex].mReadySemaphore};
	const VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 2,
		.pWaitSemaphores = waitSemaphores,
		.pWaitDstStageMask = waitPipelineStageMask,
		.commandBufferCount = 1,
		.pCommandBuffers = &mGraphicsContext.mCommandBuffers[mCurrentBufferIndex],
		.signalSemaphoreCount = 2,
		.pSignalSemaphores = signalSemaphores
	};
	VK_CHECK_RESULT(vkQueueSubmit(mGraphicsContext.mQueue, 1, &submitInfo, mGraphicsContext.mFences[mCurrentBufferIndex]));

	const VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &mGraphicsContext.mRenderCompleteSemaphores[mCurrentImageIndex],
		.swapchainCount = 1,
		.pSwapchains = &mVulkanSwapChain.mVkSwapchainKHR,
		.pImageIndices = &mCurrentImageIndex
	};

	const VkResult result = vkQueuePresentKHR(mGraphicsContext.mQueue, &presentInfo);
	// Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)
	if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR) || mWindow.lock()->GetWindowProperties().mIsFramebufferResized)
	{
		mWindow.lock()->OnFramebufferResizeProcessed();

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

void VulkanRenderer::SubmitFrameCompute()
{
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::SubmitFrameCompute");

	const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	const VkSubmitInfo submitInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &mComputeContext.mSemaphores[((int)mCurrentBufferIndex - 1) % gMaxConcurrentFrames].mReadySemaphore,
		.pWaitDstStageMask = &waitDstStageMask,
		.commandBufferCount = 1,
		.pCommandBuffers = &mComputeContext.mCommandBuffers[mCurrentBufferIndex],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &mComputeContext.mSemaphores[mCurrentBufferIndex].mCompleteSemaphore,
	};
	VK_CHECK_RESULT(vkQueueSubmit(mComputeContext.mQueue, 1, &submitInfo, mComputeContext.mFences[mCurrentBufferIndex]));
}

void VulkanRenderer::CreateVkInstance()
{
	mRequestedInstanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	mRequestedInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	Core::uint32 extensionCount = 0;
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

	const std::vector<const char*> glfwRequiredExtensions = mWindow.lock()->GetGlfwRequiredExtensions();
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
		instanceCreateInfo.enabledExtensionCount = static_cast<Core::uint32>(mInstanceExtensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = mInstanceExtensions.data();

#ifdef _DEBUG
		for (const char* instanceExtension : mInstanceExtensions)
		{
			std::cout << "Enabling instance extension " << instanceExtension << std::endl;
		}
#endif
	}

	if (mEngineProperties->mIsValidationEnabled)
	{
		Core::uint32 instanceLayerCount;
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
			.settingCount = static_cast<Core::uint32>(mEnabledLayerSettings.size()),
			.pSettings = mEnabledLayerSettings.data(),
		};
		instanceCreateInfo.pNext = &layerSettingsCreateInfo;
	}

	const VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error(std::format("Could not create Vulkan instance: {}", VulkanTools::GetErrorString(result)));
	}

	mWindow.lock()->CreateWindowSurface(&mInstance, &mVulkanSwapChain.mVkSurfaceKHR);

	// If the debug utils extension is present we set up debug functions, so samples can label objects for debugging
	if (std::find(mSupportedInstanceExtensions.begin(), mSupportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != mSupportedInstanceExtensions.end())
	{
		VulkanDebug::SetupDebugUtils(mInstance);
	}
}

void VulkanRenderer::CreateVulkanDevice()
{
	Core::uint32 physicalDeviceCount = 0;
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, nullptr));
	if (physicalDeviceCount == 0)
	{
		throw std::runtime_error(std::format("No device with Vulkan support found: {}", VulkanTools::GetErrorString(VK_ERROR_DEVICE_LOST)));
	}

	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, physicalDevices.data()));

	Core::uint32 selectedDevice = 0;
	VkPhysicalDevice vkPhysicalDevice = physicalDevices[selectedDevice];
	mVulkanDevice = new VulkanDevice();
	mVulkanDevice->CreatePhysicalDevice(vkPhysicalDevice);
	mVulkanDevice->CreateLogicalDevice(mEnabledDeviceExtensions, &mPhysicalDevice13Features, true, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
}

void VulkanRenderer::CreatePipelineCache()
{
	const VkPipelineCacheCreateInfo vkPipelineCacheCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
	};
	VK_CHECK_RESULT(vkCreatePipelineCache(mVulkanDevice->mLogicalVkDevice, &vkPipelineCacheCreateInfo, nullptr, &mPipelineCache));
}

void VulkanRenderer::PrepareIndirectData()
{
	mIndirectDrawCount = gModelInstanceCount * gModelInstanceCount * gModelInstanceCount;
	mIndirectCommands.resize(mIndirectDrawCount);

	for (Core::uint8 x = 0; x < gModelInstanceCount; x++)
	{
		for (Core::uint8 y = 0; y < gModelInstanceCount; y++)
		{
			for (Core::uint8 z = 0; z < gModelInstanceCount; z++)
			{
				const Core::uint32 index = x + y * gModelInstanceCount + z * gModelInstanceCount * gModelInstanceCount;
				mIndirectCommands[index].instanceCount = 1;
				mIndirectCommands[index].firstInstance = index;
				// firstIndex and indexCount are written by the compute shader
			}
		}
	}

	mIndrectDrawInfo.mDrawCount = static_cast<Core::uint32>(mIndirectCommands.size());

	Buffer stagingBuffer;
	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		mIndirectCommands.size() * sizeof(VkDrawIndexedIndirectCommand),
		mIndirectCommands.data()));

	for (Buffer& indirectCommandsBuffer : mIndirectCommandsBuffers)
	{
		VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&indirectCommandsBuffer,
			stagingBuffer.mVkDeviceSize));

		mVulkanDevice->CopyBuffer(&stagingBuffer, &indirectCommandsBuffer, mGraphicsContext.mQueue);

		// Add an initial release barrier to the graphics queue,
		// so that when the compute command buffer executes for the first time
		// it doesn't complain about a lack of a corresponding "release" to its "acquire"
		VkCommandBuffer barrierCommandBuffer = mVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		if (mVulkanDevice->mQueueFamilyIndices.mGraphics != mVulkanDevice->mQueueFamilyIndices.mCompute)
		{
			const VkBufferMemoryBarrier bufferMemoryBarrier =
			{
				VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				nullptr,
				VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
				0,
				mVulkanDevice->mQueueFamilyIndices.mGraphics,
				mVulkanDevice->mQueueFamilyIndices.mCompute,
				indirectCommandsBuffer.mVkBuffer,
				0,
				indirectCommandsBuffer.mVkDescriptorBufferInfo.range
			};
			vkCmdPipelineBarrier(
				barrierCommandBuffer,
				VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				0,
				0,
				nullptr,
				1, 
				&bufferMemoryBarrier,
				0, 
				nullptr);
		}

		mVulkanDevice->FlushCommandBuffer(barrierCommandBuffer, mGraphicsContext.mQueue, true);
	}

	stagingBuffer.Destroy();
}

void VulkanRenderer::PrepareInstanceData()
{
	std::vector<InstanceData> instanceData(mIndirectDrawCount);

	for (Core::uint8 x = 0; x < gModelInstanceCount; x++)
	{
		for (Core::uint8 y = 0; y < gModelInstanceCount; y++)
		{
			for (Core::uint8 z = 0; z < gModelInstanceCount; z++)
			{
				const Core::uint32 index = x + y * gModelInstanceCount + z * gModelInstanceCount * gModelInstanceCount;
				instanceData[index].mPosition = Math::Vector3f((float)x, (float)y, (float)z) - Math::Vector3f((float)gModelInstanceCount / 2.0f);
				instanceData[index].mScale = 2.0f;
			}
		}
	}

	Buffer stagingBuffer;
	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		instanceData.size() * sizeof(InstanceData),
		instanceData.data()));

	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&mInstanceBuffer,
		stagingBuffer.mVkDeviceSize));

	mVulkanDevice->CopyBuffer(&stagingBuffer, &mInstanceBuffer, mGraphicsContext.mQueue);

	stagingBuffer.Destroy();

	// Draw count buffer for host side info readback
	for (Buffer& indirectDrawCountBuffer : mIndirectDrawCountBuffers)
	{
		VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&indirectDrawCountBuffer,
			sizeof(mIndrectDrawInfo)));

		VK_CHECK_RESULT(indirectDrawCountBuffer.Map());
	}

	// Shader storage buffer containing index offsets and counts for the LODs
	struct LOD
	{
		Core::uint32 firstIndex;
		Core::uint32 indexCount;
		float distance;
		float _pad0;
	};
	std::vector<LOD> LODLevels;

	Core::uint32 nodeIndex = 0;
	for (const vkglTF::Node* node : mModelManager->GetModel(mModelIdentifiers.mSuzanneModelIdentifier)->nodes)
	{
		LOD lod{};
		lod.firstIndex = node->mMesh->mPrimitives[0]->firstIndex; // First index for this LOD
		lod.indexCount = node->mMesh->mPrimitives[0]->indexCount; // Index count for this LOD
		lod.distance = 5.0f + nodeIndex * 5.0f; // Starting distance (to viewer) for this LOD
		nodeIndex++;
		LODLevels.push_back(lod);
	}

	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer,
		LODLevels.size() * sizeof(LOD),
		LODLevels.data()));

	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&mComputeContext.mLoDBuffers,
		stagingBuffer.mVkDeviceSize));

	mVulkanDevice->CopyBuffer(&stagingBuffer, &mComputeContext.mLoDBuffers, mGraphicsContext.mQueue);

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

	mShaderModules.push_back(pipelineShaderStageCreateInfo.module);
	return pipelineShaderStageCreateInfo;
}

void VulkanRenderer::DrawNode(const vkglTF::Node* aNode, VkCommandBuffer aCommandBuffer, RenderFlags aRenderFlags, VkPipelineLayout aPipelineLayout, Core::uint32 aBindImageSet)
{
	if (aNode->mMesh)
	{
		for (const vkglTF::Primitive* primitive : aNode->mMesh->mPrimitives)
		{
			bool shouldSkipPrimitive = false;
			const vkglTF::Material& material = primitive->material;
			if ((aRenderFlags & RenderFlags::RenderOpaqueNodes) == RenderFlags::RenderOpaqueNodes)
			{
				shouldSkipPrimitive = (material.mAlphaMode != vkglTF::Material::AlphaMode::Opaque);
			}

			if ((aRenderFlags & RenderFlags::RenderAlphaMaskedNodes) == RenderFlags::RenderAlphaMaskedNodes)
			{
				shouldSkipPrimitive = (material.mAlphaMode != vkglTF::Material::AlphaMode::Mask);
			}

			if ((aRenderFlags & RenderFlags::RenderAlphaBlendedNodes) == RenderFlags::RenderAlphaBlendedNodes)
			{
				shouldSkipPrimitive = (material.mAlphaMode != vkglTF::Material::AlphaMode::Blend);
			}

			if (!shouldSkipPrimitive)
			{
				if ((aRenderFlags & RenderFlags::BindImages) == RenderFlags::BindImages)
				{
					vkCmdBindDescriptorSets(aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aPipelineLayout, aBindImageSet, 1, &material.mDescriptorSet, 0, nullptr);
				}

				vkCmdDrawIndexed(aCommandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
			}
		}
	}

	for (const vkglTF::Node* child : aNode->mChildren)
	{
		DrawNode(child, aCommandBuffer, aRenderFlags, aPipelineLayout, aBindImageSet);
	}
}

void VulkanRenderer::DrawModel(vkglTF::Model* aModel, VkCommandBuffer aCommandBuffer, RenderFlags aRenderFlags, VkPipelineLayout aPipelineLayout, Core::uint32 aBindImageSet)
{
	BindModelBuffers(aModel, aCommandBuffer);

	for (const vkglTF::Node* node : aModel->nodes)
	{
		DrawNode(node, aCommandBuffer, aRenderFlags, aPipelineLayout, aBindImageSet);
	}
}

void VulkanRenderer::BindModelBuffers(vkglTF::Model* aModel, VkCommandBuffer aCommandBuffer)
{
	const VkDeviceSize offsets[1] = {0};
	vkCmdBindVertexBuffers(aCommandBuffer, 0, 1, &aModel->vertices.mBuffer, offsets);
	vkCmdBindIndexBuffer(aCommandBuffer, aModel->indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanRenderer::RenderFrame()
{
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::RenderFrame");

	mFrameTimer->StartTimer();
	
	PrepareFrameCompute();
	BuildComputeCommandBuffer();
	SubmitFrameCompute();

	PrepareFrameGraphics();
	UpdateUniformBuffers();
	UpdateModelMatrix();
	BuildGraphicsCommandBuffer();
	SubmitFrameGraphics();

	mFrameTimer->EndTimer();

	mFrametime = static_cast<float>(mFrameTimer->GetDurationSeconds());

	mFrameCounter++;
	const float fpsTimer = static_cast<float>(Time::GetDurationMilliseconds(mFrameTimer->GetEndTime(), mLastTimestamp));
	if (fpsTimer > mFPSTimerInterval)
	{
		mAverageFPS = static_cast<Core::uint32>(static_cast<float>(mFrameCounter) * (mFPSTimerInterval / fpsTimer));
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
		VulkanDebug::SetupDebugUtilsMessenger(mInstance);
	}

	CreateVulkanDevice();

	// Get a graphics queue from the device
	vkGetDeviceQueue(mVulkanDevice->mLogicalVkDevice, mVulkanDevice->mQueueFamilyIndices.mGraphics, 0, &mGraphicsContext.mQueue);

	// Applications that make use of stencil will require a depth + stencil format
	const VkBool32 validFormat = VulkanTools::GetSupportedDepthFormat(mVulkanDevice->mVkPhysicalDevice, &mVkDepthFormat);
	if (!validFormat)
	{
		throw std::runtime_error("Invalid format");
	}

	mVulkanSwapChain.SetContext(mInstance, mVulkanDevice);
}

void VulkanRenderer::CreateGraphicsCommandPool()
{
	const VkCommandPoolCreateInfo commandPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = mVulkanSwapChain.mQueueNodeIndex,
	};
	VK_CHECK_RESULT(vkCreateCommandPool(mVulkanDevice->mLogicalVkDevice, &commandPoolCreateInfo, nullptr, &mGraphicsContext.mCommandPool));
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
	vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mDepthStencil.mVkImageView, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mDepthStencil.mVkImage, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mDepthStencil.mVkDeviceMemory, nullptr);

	SetupDepthStencil();

	if ((mFramebufferWidth > 0.0f) && (mFramebufferHeight > 0.0f))
	{
		mImGuiOverlay->Resize(mFramebufferWidth, mFramebufferHeight);
	}

	for (VkSemaphore& vkPresentCompleteSemaphore : mGraphicsContext.mPresentCompleteSemaphores)
		vkDestroySemaphore(mVulkanDevice->mLogicalVkDevice, vkPresentCompleteSemaphore, nullptr);
	
	for (VkSemaphore& vkRendercompleteSemaphore : mGraphicsContext.mRenderCompleteSemaphores)
		vkDestroySemaphore(mVulkanDevice->mLogicalVkDevice, vkRendercompleteSemaphore, nullptr);
	
	for (VkFence& waitVkFence : mGraphicsContext.mFences)
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
	SIMPLE_PROFILER_PROFILE_SCOPE("VulkanRenderer::UpdateUIOverlay");

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(static_cast<float>(mFramebufferWidth), static_cast<float>(mFramebufferHeight));
	io.DeltaTime = mFrametime;

	const bool isVisible = mImGuiOverlay->IsVisible();
	const Input::InputManager& inputManager = Input::InputManager::GetInstance();
	io.MousePos = ImVec2(inputManager.GetMousePosition().x, inputManager.GetMousePosition().y);
	io.MouseDown[0] = inputManager.GetIsMouseButtonDown(Input::MouseButton::Left) && isVisible;
	io.MouseDown[1] = inputManager.GetIsMouseButtonDown(Input::MouseButton::Right) && isVisible;
	io.MouseDown[2] = inputManager.GetIsMouseButtonDown(Input::MouseButton::Middle) && isVisible;

	ImGui::NewFrame();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Windows"))
		{
			if (ImGui::MenuItem("Editor Info", nullptr, &mShouldShowEditorInfo)) {}
			if (ImGui::MenuItem("Simple Profiler", nullptr, &mShouldShowProfiler)) {}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	OnUpdateUIOverlay();

	ImGui::PopStyleVar();
	ImGui::Render();

	mImGuiOverlay->Update(mCurrentBufferIndex);
}

void VulkanRenderer::OnUpdateUIOverlay()
{
	static vkglTF::Model* selectedModel = nullptr;
	if (mShouldShowEditorInfo)
	{
		ImGui::SetNextWindowPos(ImVec2(10.0f * mImGuiOverlay->GetScale(), 40.0f * mImGuiOverlay->GetScale()));
		ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);

		ImGui::Begin("Editor Info", &mShouldShowEditorInfo, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
		ImGui::TextUnformatted(mVulkanDevice->mVkPhysicalDeviceProperties.deviceName);
		ImGui::Text("%i/%i", mFramebufferWidth, mFramebufferHeight);
		ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / mAverageFPS), mAverageFPS);
		ImGui::PushItemWidth(160.0f * mImGuiOverlay->GetScale());

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
#ifdef _DEBUG
			if (mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.fillModeNonSolid)
				ImGui::Checkbox("Draw wireframe", &mShouldDrawWireframe);
#endif

			ImGui::Checkbox("Freeze frustum", &mShouldFreezeFrustum);

			ImGui::Text("samplerAnisotropy is %s", mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.samplerAnisotropy ? "enabled" : "disabled");
			ImGui::Text("multiDrawIndirect is %s", mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.multiDrawIndirect ? "enabled" : "disabled");
			ImGui::Text("drawIndirectFirstInstance is %s", mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.drawIndirectFirstInstance ? "enabled" : "disabled");
#ifdef _DEBUG
			ImGui::Text("fillModeNonSolid is %s", mVulkanDevice->mEnabledVkPhysicalDeviceFeatures.fillModeNonSolid ? "enabled" : "disabled");
#endif
			ImGui::Text("VSync is %s", mEngineProperties->mIsVSyncEnabled ? "enabled" : "disabled");
			ImGui::Text("Validation Layers is %s", mEngineProperties->mIsValidationEnabled ? "enabled" : "disabled");
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Scene Details", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Visible objects: %d", mIndrectDrawInfo.mDrawCount);
			for (int i = 0; i < gMaxLOD + 1; i++)
			{
				ImGui::Text("LOD %d: %d", i, mIndrectDrawInfo.mLoDCount[i]);
			}

			ImGui::NewLine();

			if (ImGui::Button("Planet"))
			{
				selectedModel = mModelManager->GetModel(mModelIdentifiers.mPlanetModelIdentifier);
				mShouldShowModelInspector = true;
			}

			if (ImGui::Button("Voyager"))
			{
				selectedModel = mModelManager->GetModel(mModelIdentifiers.mVoyagerModelIdentifier);
				mShouldShowModelInspector = true;
			}

			if (ImGui::Button("Suzanne"))
			{
				selectedModel = mModelManager->GetModel(mModelIdentifiers.mSuzanneModelIdentifier);
				mShouldShowModelInspector = true;
			}

			ImGui::InputFloat4("Light position", Math::ValuePointer(mLightPosition), "%.1f");

			const Math::Vector3f& cameraPosition = mCamera->GetPosition();
			mImGuiOverlay->Vec3Text("Camera position", cameraPosition);

			const Math::Vector3f& cameraRotaiton = mCamera->GetRotation();
			mImGuiOverlay->Vec3Text("Camera rotation", cameraRotaiton);

			const Math::Vector4f& cameraViewPosition = mCamera->GetViewPosition();
			mImGuiOverlay->Vec4Text("Camera view position", cameraViewPosition);

			ImGui::NewLine();

			mImGuiOverlay->Mat4Text("Voyager", mVoyagerModelMatrix);

			ImGui::NewLine();

			mImGuiOverlay->Mat4Text("Planet", mPlanetModelMatrix);
		}

		ImGui::PopItemWidth();
		ImGui::End();
	}

	if (mShouldShowModelInspector)
	{
		ImGui::Begin("Model Inspector", &mShouldShowModelInspector, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);
		ImGui::Text("Vertices %i", selectedModel->vertices.mCount);
		ImGui::Text("Indices %i", selectedModel->indices.mCount);

		if (ImGui::TreeNode(std::format("Textures ({})", selectedModel->textures.size()).c_str()))
		{
			for (const vkglTF::Texture& texture : selectedModel->textures)
			{
				if (ImGui::TreeNode(std::format("Index ({})", texture.mIndex).c_str()))
				{
					ImGui::BulletText("Width %u", texture.mWidth);
					ImGui::BulletText("Height %u", texture.mHeight);
					ImGui::BulletText("Mips %u", texture.mMipLevels);
					ImGui::BulletText("Layers %u", texture.mLayerCount);
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode(std::format("Materials ({})", selectedModel->materials.size()).c_str()))
		{
			int materialIndex = 0;
			for (const vkglTF::Material& material : selectedModel->materials)
			{
				if (ImGui::TreeNode(std::format("Index ({})", materialIndex).c_str()))
				{
					switch (material.mAlphaMode)
					{
						case vkglTF::Material::AlphaMode::Blend:
							ImGui::Text("Alpha mode Blend");
							break;
						case vkglTF::Material::AlphaMode::Mask:
							ImGui::Text("Alpha mode Mask");
							break;
						case vkglTF::Material::AlphaMode::Opaque:
							ImGui::Text("Alpha mode Opaque");
							break;
					}

					ImGui::Text("Alpha cutoff %f", material.mAlphaCutoff);
					ImGui::Text("Base color factor %f", material.mBaseColorFactor);
					ImGui::Text("Roughness factor %f", material.mRoughnessFactor);

					if (material.mBaseColorTexture)
						ImGui::Text("Base color texture %u", material.mBaseColorTexture->mIndex);

					if (material.mDiffuseTexture)
						ImGui::Text("Diffuse texture %u", material.mDiffuseTexture->mIndex);

					if (material.mEmissiveTexture)
						ImGui::Text("Emissive texture %u", material.mEmissiveTexture->mIndex);

					if (material.mMetallicRoughnessTexture)
						ImGui::Text("Metallic texture %u", material.mMetallicRoughnessTexture->mIndex);

					if (material.mOcclusionTexture)
						ImGui::Text("Occlusion texture %u", material.mOcclusionTexture->mIndex);

					if (material.mSpecularGlossinessTexture)
						ImGui::Text("Specular glossiness texture %u", material.mSpecularGlossinessTexture->mIndex);
					ImGui::TreePop();
				}

				++materialIndex;
			}
			ImGui::TreePop();
		}

		ImGui::End();
	}

	if (mShouldShowProfiler)
	{
		ImGui::Begin("Simple Profiler", &mShouldShowProfiler, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize);
		SimpleProfiler::ShowImguiProfiler();
		ImGui::End();
	}
}
