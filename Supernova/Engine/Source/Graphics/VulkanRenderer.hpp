#pragma once

#include "Time.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanTypes.hpp"

#include <glm/fwd.hpp>
#include <vulkan/vulkan_core.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

constexpr std::uint32_t gMaxConcurrentFrames = 2;
constexpr int gModelInstanceCount = 64;
constexpr int gMaxLOD = 5;

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
	void PrepareFrame();
	void BuildGraphicsCommandBuffer();
	void BuildComputeCommandBuffer();
	void UpdateModelMatrix();
	void UpdateUniformBuffers();
	void SubmitFrame();
	void SetupDepthStencil();

	void LoadAssets();
	void CreateSynchronizationPrimitives();
	void CreateCommandBuffers();
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
	void CreateCommandPool();
	void SetupSwapchain();
	void DrawImGuiOverlay(const VkCommandBuffer aVkCommandBuffer);
	void UpdateUIOverlay();
	void OnUpdateUIOverlay();

	struct DescriptorSets
	{
		VkDescriptorSet mInstancedRocks{VK_NULL_HANDLE};
		VkDescriptorSet mStaticPlanet{VK_NULL_HANDLE};
		VkDescriptorSet mStaticVoyager{VK_NULL_HANDLE};
	};

	struct
	{
		VkPipeline mVoyager{VK_NULL_HANDLE};
		VkPipeline mPlanet{VK_NULL_HANDLE};
		VkPipeline mPlanetWireframe{VK_NULL_HANDLE};
		VkPipeline mRocks{VK_NULL_HANDLE};
		VkPipeline mRocksWireframe{VK_NULL_HANDLE};
	} mVkPipelines{};

	struct
	{
		VulkanTexture2DArray mRockTextureArray;
		VulkanTexture2D mPlanetTexture;
	} mTextures{};

	struct
	{
		vkglTF::Model* mVoyagerModel{nullptr};
		vkglTF::Model* mRockModel{nullptr};
		vkglTF::Model* mPlanetModel{nullptr};
	} mModels{};

	struct
	{
		uint32_t drawCount;						// Total number of indirect draw counts to be issued
		uint32_t lodCount[gMaxLOD + 1];	// Statistics for number of draws per LOD level (written by compute shader)
	} indirectStats{};

	struct Compute
	{
		VulkanBuffer lodLevelsBuffers;										// Contains index start and counts for the different lod levels
		VkQueue queue;														// Separate queue for compute commands (queue family may differ from the one used for graphics)
		VkCommandPool commandPool;											// Use a separate command pool (queue family may differ from the one used for graphics)
		std::array<VkCommandBuffer, gMaxConcurrentFrames> commandBuffers;	// Command buffer storing the dispatch commands and barriers
		std::array<VkFence, gMaxConcurrentFrames> fences;					// Synchronization fence to avoid rewriting compute CB if still in use
		struct ComputeSemaphores
		{
			VkSemaphore ready{VK_NULL_HANDLE};
			VkSemaphore complete{VK_NULL_HANDLE};
		};
		std::array<ComputeSemaphores, gMaxConcurrentFrames> semaphores{};	// Used as a wait semaphore for graphics submission
		VkDescriptorSetLayout descriptorSetLayout;							// Compute shader binding layout
		std::array<VkDescriptorSet, gMaxConcurrentFrames> descriptorSets{};	// Compute shader bindings
		VkPipelineLayout pipelineLayout;									// Layout of the compute pipeline
		VkPipeline pipeline;												// Compute pipeline
	} compute{};

	VulkanFrustum mFrustum;
	VulkanUniformData mVulkanUniformData{};
	VulkanBuffer mInstanceBuffer{};
	VkPhysicalDeviceVulkan13Features mVkPhysicalDevice13Features;
	VulkanDepthStencil mVulkanDepthStencil;
	VkInstance mVkInstance; // Vulkan instance, stores all per-application states
	VkQueue mVkQueue; // Handle to the device graphics queue that command buffers are submitted to
	VkDescriptorPool mVkDescriptorPool; // Descriptor set pool
	VkPipelineCache mVkPipelineCache; // Pipeline cache object
	VulkanSwapChain mVulkanSwapChain; // Wraps the swap chain to present images (framebuffers) to the windowing system
	VkPipelineLayout mVkPipelineLayout;
	VkDescriptorSetLayout mGraphicsDescriptorSetLayout;
	VkCommandPool mVkCommandPoolBuffer;
	VulkanPushConstant mVulkanPushConstant{};
	Time::TimePoint mLastTimestamp;
	std::vector<VkDrawIndexedIndirectCommand> indirectCommands; // Store the indirect draw commands containing index offsets and instance count per object
	std::vector<std::string> mSupportedInstanceExtensions{};
	std::vector<const char*> mEnabledDeviceExtensions{}; // Set of device extensions to be enabled for this example
	std::vector<const char*> mRequestedInstanceExtensions{}; // Set of instance extensions to be enabled for this example
	std::vector<VkLayerSettingEXT> mEnabledLayerSettings{}; // Set of layer settings to be enabled for this example
	std::vector<const char*> mInstanceExtensions{}; // Set of active instance extensions
	std::vector<VkShaderModule> mVkShaderModules{}; // List of shader modules created (stored for cleanup)
	std::vector<VkSemaphore> mVkPresentCompleteSemaphores{};
	std::vector<VkSemaphore> mVkRenderCompleteSemaphores{};
	std::array<VulkanBuffer, gMaxConcurrentFrames> mVulkanUniformBuffers;
	std::array<VkCommandBuffer, gMaxConcurrentFrames> mVkCommandBuffers{}; // Command buffers used for rendering
	std::array<VkFence, gMaxConcurrentFrames> mWaitVkFences{};
	std::array<DescriptorSets, gMaxConcurrentFrames> mVkDescriptorSets{};
	std::array<VulkanBuffer, gMaxConcurrentFrames> indirectCommandsBuffers;
	std::array<VulkanBuffer, gMaxConcurrentFrames> indirectDrawCountBuffers;
	std::uint32_t mFramebufferWidth;
	std::uint32_t mFramebufferHeight;
	std::uint32_t mFrameCounter;
	std::uint32_t mAverageFPS;
	std::uint32_t mBufferIndexCount;
	std::uint32_t mCurrentImageIndex;
	std::uint32_t mCurrentBufferIndex;
	std::uint32_t mIndirectDrawCount;
	glm::mat4 mVoyagerModelMatrix;
	glm::mat4 mPlanetModelMatrix;
	glm::vec4 mClearColor;
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
#ifdef _DEBUG
	bool mShouldDrawWireframe;
#endif
};
