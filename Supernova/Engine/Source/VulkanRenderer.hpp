#pragma once

#include "Camera.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanTypes.hpp"

#include <glm/fwd.hpp>
#include <vulkan/vulkan_core.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

constexpr std::uint32_t gMaxConcurrentFrames = 2;

namespace vkglTF
{
	class Model;
}

struct EngineProperties;
class Window;

class VulkanRenderer
{
public:
	VulkanRenderer(EngineProperties* aEngineProperties, Window* aWindow);
	~VulkanRenderer();

	void InitializeRenderer();
	void PrepareUpdate();
	void UpdateRenderer(float aDeltaTime);

private:
	void PrepareVulkanResources();
	void PrepareFrame();
	void BuildCommandBuffer();
	void UpdateUniformBuffers();
	void SubmitFrame();
	void SetupDepthStencil();

	void LoadAssets();
	void CreateSynchronizationPrimitives();
	void CreateCommandBuffers();
	void CreateDescriptors();
	VkShaderModule LoadSPIRVShader(const std::filesystem::path& aPath) const;
	void CreatePipeline();
	void CreateUniformBuffers();

	void InitializeVulkan();
	void CreateVkInstance();
	void CreateVulkanDevice();

	VkPipelineShaderStageCreateInfo LoadShader(const std::filesystem::path& aPath, VkShaderStageFlagBits aVkShaderStageMask);

	void OnResizeWindow();

	std::string GetWindowTitle() const;
	void NextFrame();
	void CreatePipelineCache();
	void InitializeSwapchain();
	void CreateCommandPool();
	void SetupSwapchain();

	Camera mCamera;
	VulkanUniformData mVulkanUniformData;
	VkPhysicalDeviceVulkan13Features mVkPhysicalDevice13Features;
	VulkanDepthStencil mVulkanDepthStencil;
	VkInstance mVkInstance; // Vulkan instance, stores all per-application states
	VkQueue mVkQueue; // Handle to the device graphics queue that command buffers are submitted to
	VkDescriptorPool mVkDescriptorPool; // Descriptor set pool
	VkPipelineCache mVkPipelineCache; // Pipeline cache object
	VulkanSwapChain mVulkanSwapChain; // Wraps the swap chain to present images (framebuffers) to the windowing system
	VkPipelineLayout mVkPipelineLayout;
	VkPipeline mVkPipeline;
	VkPipeline mStarfieldVkPipeline;
	VkDescriptorSetLayout mVkDescriptorSetLayout;
	VkCommandPool mVkCommandPoolBuffer;
	std::filesystem::path mModelPath;
	std::filesystem::path mVertexShaderPath;
	std::filesystem::path mFragmentShaderPath;
	std::filesystem::path mStarfieldVertexShaderPath;
	std::filesystem::path mStarfieldFragmentShaderPath;
	std::chrono::time_point<std::chrono::high_resolution_clock> mLastTimestamp;
	std::chrono::time_point<std::chrono::high_resolution_clock> mPreviousEndTime;
	std::array<VulkanBuffer, gMaxConcurrentFrames> mVulkanUniformBuffers;
	std::vector<std::string> mSupportedInstanceExtensions{};
	std::vector<const char*> mEnabledDeviceExtensions{}; // Set of device extensions to be enabled for this example
	std::vector<const char*> mRequestedInstanceExtensions{}; // Set of instance extensions to be enabled for this example
	std::vector<VkLayerSettingEXT> mEnabledLayerSettings{}; // Set of layer settings to be enabled for this example
	std::vector<const char*> mInstanceExtensions{}; // Set of active instance extensions
	std::vector<VkShaderModule> mVkShaderModules{}; // List of shader modules created (stored for cleanup)
	std::vector<VkSemaphore> mVkPresentCompleteSemaphores{};
	std::vector<VkSemaphore> mVkRenderCompleteSemaphores{};
	std::vector<float> mFrametimes{};
	std::array<VkCommandBuffer, gMaxConcurrentFrames> mVkCommandBuffers{}; // Command buffers used for rendering
	std::array<VkFence, gMaxConcurrentFrames> mWaitVkFences{};
	std::array<VkDescriptorSet, gMaxConcurrentFrames> mVkDescriptorSets{};
	std::uint32_t mFramebufferWidth;
	std::uint32_t mFramebufferHeight;
	std::uint32_t mFrameCounter;
	std::uint32_t mLastFPS;
	std::uint32_t mMaxFrametimes;
	std::uint32_t mBufferIndexCount;
	std::uint32_t mCurrentImageIndex;
	std::uint32_t mCurrentBufferIndex;
	vkglTF::Model* mGlTFModel;
	EngineProperties* mEngineProperties;
	Window* mWindow;
	VulkanDevice* mVulkanDevice; // Encapsulated physical and logical vulkan device
	VkFormat mVkDepthFormat; // Depth buffer format (selected during Vulkan initialization)
	float mFrametime; // Last frame time measured using a high performance timer (if available)
	float mTimer; // Defines a frame rate independent timer value clamped from -1.0...1.0
	float mAverageFrametime;
	bool mShouldClose;
	bool mIsPrepared;
};
