#pragma once

#include "Camera.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanTypes.hpp"
#include "VulkanGlTFModel.hpp"

#include <glm/fwd.hpp>
#include <vulkan/vulkan_core.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

constexpr std::uint32_t gMaxConcurrentFrames = 2;

typedef struct GLFWwindow GLFWwindow;

class VulkanRenderer
{
public:
	VulkanRenderer();
	~VulkanRenderer();

	void InitializeRenderer();
	void PrepareUpdate();
	void UpdateRenderer(float aDeltaTime);
	void DestroyRenderer();

	bool ShouldClose() const { return mShouldClose; }
	bool IsPaused() const { return mIsPaused; }

private:
	std::array<VulkanBuffer, gMaxConcurrentFrames> uniformBuffers;

	// Synchronization related objects and variables
	// These are used to have multiple frame buffers "in flight" to get some CPU/GPU parallelism
	uint32_t currentImageIndex{0};
	uint32_t currentBuffer{0};

	// Command buffer pool
	VkCommandPool mVkCommandPoolBuffer{VK_NULL_HANDLE};

	void PrepareVulkanResources();
	void PrepareFrame();
	void buildCommandBuffer();
	void updateUniformBuffers();
	void submitFrame();
	void SetupDepthStencil();

	void loadAssets();
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
	void SetWindowIcon(unsigned char* aSource, int aWidth, int aHeight) const;

	void OnResizeWindow();

	void CreateGlfwWindow();
	void SetWindowSize(int aWidth, int aHeight);

	std::string GetWindowTitle() const;
	void NextFrame();
	void CreatePipelineCache();
	void InitializeSwapchain();
	void CreateCommandPool();
	void SetupSwapchain();

	static void KeyCallback(GLFWwindow* aWindow, int aKey, int aScancode, int aAction, int aMode);
	static void FramebufferResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
	static void WindowResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
	static void WindowMinimizedCallback(GLFWwindow* aWindow, int aValue);

	Camera mCamera;
	VulkanUniformData mVulkanUniformData;
	VkPhysicalDeviceVulkan13Features mVkPhysicalDevice13Features;
	VulkanDepthStencil mVulkanDepthStencil;
	VkInstance mVkInstance; // Vulkan instance, stores all per-application states
	VkQueue mVkQueue; // Handle to the device graphics queue that command buffers are submitted to
	VkDescriptorPool mVkDescriptorPool; // Descriptor set pool
	VkPipelineCache mVkPipelineCache; // Pipeline cache object
	VulkanSwapChain mVulkanSwapChain; // Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanApplicationProperties mVulkanApplicationProperties;
	VkPipelineLayout mVkPipelineLayout;
	VkPipeline mVkPipeline;
	VkDescriptorSetLayout mVkDescriptorSetLayout;
	std::filesystem::path mIconPath;
	std::filesystem::path mModelPath;
	std::filesystem::path mVertexShaderPath;
	std::filesystem::path mFragmentShaderPath;
	std::chrono::time_point<std::chrono::high_resolution_clock> mLastTimestamp;
	std::chrono::time_point<std::chrono::high_resolution_clock> mPreviousEndTime;
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
	vkglTF::Model* mGlTFModel;
	VulkanDevice* mVulkanDevice; // Encapsulated physical and logical vulkan device
	GLFWwindow* mGLFWWindow;
	VkFormat mVkDepthFormat; // Depth buffer format (selected during Vulkan initialization)
	float mFrametime; // Last frame time measured using a high performance timer (if available)
	float mTimer; // Defines a frame rate independent timer value clamped from -1.0...1.0
	float mTimerSpeed; // Multiplier for speeding up (or slowing down) the global timer
	float mAverageFrametime;
	bool mShouldClose;
	bool mIsFramebufferResized;
	bool mIsPaused;
	bool mIsPrepared;
	bool mIsResized;
};
