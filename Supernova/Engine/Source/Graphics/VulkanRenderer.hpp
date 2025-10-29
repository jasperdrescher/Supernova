#pragma once

#include "Core/Types.hpp"
#include "Math/Types.hpp"
#include "Time.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanTypes.hpp"

#include <vulkan/vulkan_core.h>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace vkglTF
{
	class Model;
}

namespace Time
{
	struct Timer;
}

struct EngineProperties;
class Camera;
class Window;
class ImGuiOverlay;

class VulkanRenderer
{
public:
	VulkanRenderer(EngineProperties* aEngineProperties, Window* aWindow);
	~VulkanRenderer();

	void InitializeRenderer();
	void PrepareUpdate();
	void EndUpdate();
	void UpdateRenderer(float aDeltaTime);

private:
	void PrepareVulkanResources();
	void PrepareFrameGraphics();
	void BuildGraphicsCommandBuffer();
	void PrepareFrameCompute();
	void BuildComputeCommandBuffer();
	void UpdateModelMatrix();
	void UpdateUniformBuffers();
	void SubmitFrameGraphics();
	void SubmitFrameCompute();
	void SetupDepthStencil();

	void LoadAssets();
	void CreateSynchronizationPrimitives();
	void CreateGraphicsCommandBuffers();
	void CreateDescriptorPool();
	void CreateGraphicsDescriptorSetLayout();
	void CreateGraphicsDescriptorSets();
	void CreateGraphicsPipelines();
	void CreateComputeDescriptorSetLayout();
	void CreateComputeDescriptorSets();
	void CreateComputePipelines();
	void CreateUniformBuffers();
	void CreateUIOverlay();

	void InitializeVulkan();
	void CreateVkInstance();
	void CreateVulkanDevice();

	VkPipelineShaderStageCreateInfo LoadShader(const std::filesystem::path& aPath, VkShaderStageFlagBits aVkShaderStageMask);

	void OnResizeWindow();

	void RenderFrame();
	void CreatePipelineCache();
	void PrepareIndirectData();
	void PrepareInstanceData();
	void InitializeSwapchain();
	void CreateGraphicsCommandPool();
	void SetupSwapchain();
	void DrawImGuiOverlay(const VkCommandBuffer aVkCommandBuffer);
	void UpdateUIOverlay();
	void OnUpdateUIOverlay();

	struct DescriptorSets
	{
		VkDescriptorSet mSuzanneModel{VK_NULL_HANDLE};
		VkDescriptorSet mStaticPlanet{VK_NULL_HANDLE};
		VkDescriptorSet mStaticVoyager{VK_NULL_HANDLE};
	};

	struct
	{
		VkPipeline mVoyager{VK_NULL_HANDLE};
		VkPipeline mPlanet{VK_NULL_HANDLE};
		VkPipeline mPlanetWireframe{VK_NULL_HANDLE};
		VkPipeline mInstancedSuzanne{VK_NULL_HANDLE};
		VkPipeline mInstancedSuzanneWireframe{VK_NULL_HANDLE};
	} mVkPipelines{};

	struct
	{
		VulkanTexture2D mPlanetTexture;
	} mTextures{};

	struct
	{
		vkglTF::Model* mVoyagerModel{nullptr};
		vkglTF::Model* mSuzanneModel{nullptr};
		vkglTF::Model* mPlanetModel{nullptr};
	} mModels{};

	struct
	{
		Core::uint32 mDrawCount; // Total number of indirect draw counts to be issued
		Core::uint32 mLoDCount[gMaxLOD + 1]; // Statistics for number of draws per LOD level (written by compute shader)
	} mIndrectDrawInfo{};

	GraphicsContext mGraphicsContext{};
	ComputeContext mComputeContext{};
	ViewFrustum mViewFrustum{};
	UniformBufferData mUniformBufferData{};
	Buffer mInstanceBuffer{};
	VkPhysicalDeviceVulkan13Features mPhysicalDevice13Features;
	DepthStencil mDepthStencil;
	VkInstance mInstance; // Vulkan instance, stores all per-application states
	VkDescriptorPool mDescriptorPool; // Descriptor set pool
	VkPipelineCache mPipelineCache; // Pipeline cache object
	VulkanSwapChain mVulkanSwapChain; // Wraps the swap chain to present images (framebuffers) to the windowing system
	PushConstant mPushConstant{};
	Time::TimePoint mLastTimestamp;
	std::vector<VkDrawIndexedIndirectCommand> mIndirectCommands; // Store the indirect draw commands containing index offsets and instance count per object
	std::vector<std::string> mSupportedInstanceExtensions{};
	std::vector<const char*> mEnabledDeviceExtensions{}; // Set of device extensions to be enabled for this example
	std::vector<const char*> mRequestedInstanceExtensions{}; // Set of instance extensions to be enabled for this example
	std::vector<VkLayerSettingEXT> mEnabledLayerSettings{}; // Set of layer settings to be enabled for this example
	std::vector<const char*> mInstanceExtensions{}; // Set of active instance extensions
	std::vector<VkShaderModule> mShaderModules{}; // List of shader modules created (stored for cleanup)
	std::array<DescriptorSets, gMaxConcurrentFrames> mDescriptorSets{};
	std::array<Buffer, gMaxConcurrentFrames> mVulkanUniformBuffers;
	std::array<Buffer, gMaxConcurrentFrames> mIndirectCommandsBuffers;
	std::array<Buffer, gMaxConcurrentFrames> mIndirectDrawCountBuffers;
	Core::uint32 mFramebufferWidth;
	Core::uint32 mFramebufferHeight;
	Core::uint32 mFrameCounter;
	Core::uint32 mAverageFPS;
	Core::uint32 mBufferIndexCount;
	Core::uint32 mCurrentImageIndex;
	Core::uint32 mCurrentBufferIndex;
	Core::uint32 mIndirectDrawCount;
	Math::Matrix4f mVoyagerModelMatrix;
	Math::Matrix4f mPlanetModelMatrix;
	Math::Vector4f mClearColor;
	Math::Vector4f mLightPosition;
	Time::Timer* mFrameTimer;
	Camera* mCamera;
	ImGuiOverlay* mImGuiOverlay;
	EngineProperties* mEngineProperties;
	Window* mWindow;
	VulkanDevice* mVulkanDevice; // Encapsulated physical and logical vulkan device
	VkFormat mVkDepthFormat; // Depth buffer format (selected during Vulkan initialization)
	float mFrametime;
	float mFPSTimerInterval;
	bool mShouldShowEditorInfo;
	bool mShouldShowProfiler;
	bool mShouldShowModelInspector;
	bool mShouldFreezeFrustum;
#ifdef _DEBUG
	bool mShouldDrawWireframe;
#endif
};
