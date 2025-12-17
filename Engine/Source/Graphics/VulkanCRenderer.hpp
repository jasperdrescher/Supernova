#pragma once

#ifdef VULKAN_C

#include "Core/Types.hpp"
#include "Math/Types.hpp"
#include "ModelFlags.hpp"
#include "Time.hpp"
#include "UniqueIdentifier.hpp"
#include "VulkanCDevice.hpp"
#include "VulkanCGlTFTypes.hpp"
#include "VulkanCSwapChain.hpp"
#include "VulkanCTypes.hpp"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkglTF
{
	struct Model;
	struct Node;
	struct Texture;
}

namespace Time
{
	struct Timer;
}

struct EngineProperties;
class Camera;
class Window;
class VulkanCImGuiOverlay;
class VulkanCTextureManager;
class VulkanCModelManager;

class VulkanCRenderer
{
public:
	VulkanCRenderer(const std::shared_ptr<EngineProperties>& aEngineProperties, const std::shared_ptr<Window>& aWindow);
	~VulkanCRenderer();

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

	void DrawNode(const vkglTF::Node* aNode, VkCommandBuffer aCommandBuffer, RenderFlags aRenderFlags, VkPipelineLayout aPipelineLayout, Core::uint32 aBindImageSet);
	void DrawModel(vkglTF::Model* aModel, VkCommandBuffer aCommandBuffer, RenderFlags aRenderFlags = RenderFlags::None, VkPipelineLayout aPipelineLayout = VK_NULL_HANDLE, Core::uint32 aBindImageSet = 1);
	void BindModelBuffers(vkglTF::Model* aModel, VkCommandBuffer aCommandBuffer);
	void RenderFrame();
	void CreatePipelineCache();
	void PrepareIndirectData();
	void PrepareInstanceData();
	void InitializeSwapchain();
	void CreateGraphicsCommandPool();
	void SetupSwapchain();
	void DrawModels(VkCommandBuffer aCommandBuffer);
	void DrawImGuiOverlay(VkCommandBuffer aCommandBuffer);
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
		vkglTF::Texture mPlanetTexture;
	} mTextures{};

	struct
	{
		UniqueIdentifier mVoyagerModelIdentifier;
		UniqueIdentifier mSuzanneModelIdentifier;
		UniqueIdentifier mPlanetModelIdentifier;
	} mModelIdentifiers{};

	struct
	{
		Core::uint32 mDrawCount; // Total number of indirect draw counts to be issued
		Core::uint32 mLoDCount[gMaxLOD + 1]; // Statistics for number of draws per LOD level (written by compute shader)
	} mIndrectDrawInfo{};

	VulkanCTypes::GraphicsContext mGraphicsContext{};
	VulkanCTypes::ComputeContext mComputeContext{};
	VulkanCTypes::ViewFrustum mViewFrustum{};
	VulkanCTypes::UniformBufferData mUniformBufferData{};
	VulkanCTypes::Buffer mInstanceBuffer{};
	VkPhysicalDeviceVulkan13Features mPhysicalDevice13Features;
	VulkanCTypes::DepthStencil mDepthStencil;
	VkInstance mInstance; // Vulkan instance, stores all per-application states
	VkDescriptorPool mDescriptorPool; // Descriptor set pool
	VkPipelineCache mPipelineCache; // Pipeline cache object
	VulkanCSwapChain mVulkanSwapChain; // Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanCTypes::PushConstant mPushConstant{};
	Time::TimePoint mLastTimestamp;
	std::vector<VkDrawIndexedIndirectCommand> mIndirectCommands; // Store the indirect draw commands containing index offsets and instance count per object
	std::vector<std::string> mSupportedInstanceExtensions{};
	std::vector<const char*> mEnabledDeviceExtensions{}; // Set of device extensions to be enabled for this example
	std::vector<const char*> mRequestedInstanceExtensions{}; // Set of instance extensions to be enabled for this example
	std::vector<VkLayerSettingEXT> mEnabledLayerSettings{}; // Set of layer settings to be enabled for this example
	std::vector<const char*> mInstanceExtensions{}; // Set of active instance extensions
	std::vector<VkShaderModule> mShaderModules{}; // List of shader modules created (stored for cleanup)
	std::array<DescriptorSets, gMaxConcurrentFrames> mDescriptorSets{};
	std::array<VulkanCTypes::Buffer, gMaxConcurrentFrames> mVulkanUniformBuffers;
	std::array<VulkanCTypes::Buffer, gMaxConcurrentFrames> mIndirectCommandsBuffers;
	std::array<VulkanCTypes::Buffer, gMaxConcurrentFrames> mIndirectDrawCountBuffers;
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
	std::unique_ptr<Time::Timer> mFrameTimer;
	std::unique_ptr<Camera> mCamera;
	std::unique_ptr<VulkanCImGuiOverlay> mImGuiOverlay;
	std::weak_ptr<EngineProperties> mEngineProperties;
	std::weak_ptr<Window> mWindow;
	std::shared_ptr<VulkanCTextureManager> mTextureManager;
	std::unique_ptr<VulkanCModelManager> mModelManager;
	VulkanCDevice* mVulkanDevice; // Encapsulated physical and logical vulkan device
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
#endif
