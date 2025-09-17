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
	void PrepareVulkanResources();
	void PrepareFrame();
	void SetupDepthStencil();

	std::uint32_t GetMemoryTypeIndex(std::uint32_t aTypeBits, VkMemoryPropertyFlags aProperties) const;
	void CreateSynchronizationPrimitives();
	void CreateCommandBuffers();
	void CreateVertexBuffer();
	void CreateDescriptors();
	VkShaderModule LoadSPIRVShader(const std::string& aFilename) const;
	void CreatePipeline();
	void CreateUniformBuffers();

	void InitializeVulkan();
	void CreateVkInstance();
	void CreateVulkanDevice();

	VkPipelineShaderStageCreateInfo LoadShader(std::string aFilename, VkShaderStageFlagBits aVkShaderStageMask);

	void OnResizeWindow();

	std::string GetShadersPath() const;

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
	VkPhysicalDeviceVulkan13Features mVkPhysicalDevice13Features;
	VulkanDepthStencil mVulkanDepthStencil;
	VkClearColorValue mDefaultClearColor;
	VkInstance mVkInstance; // Vulkan instance, stores all per-application states
	VkQueue mVkQueue; // Handle to the device graphics queue that command buffers are submitted to
	VkCommandPool mVkCommandPool; // Command buffer pool
	VkDescriptorPool mVkDescriptorPool; // Descriptor set pool
	VkPipelineCache mVkPipelineCache; // Pipeline cache object
	VulkanSwapChain mVulkanSwapChain; // Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanApplicationProperties mVulkanApplicationProperties;
	VulkanBuffer mVulkanVertexBuffer;
	VulkanBuffer mVulkanIndexBuffer;
	VkPipelineLayout mVkPipelineLayout;
	VkPipeline mVkPipeline;
	VkDescriptorSetLayout mVkDescriptionSetLayout;
	std::chrono::time_point<std::chrono::high_resolution_clock> mLastTimestamp;
	std::chrono::time_point<std::chrono::high_resolution_clock> mPreviousEndTime;
	std::vector<std::string> mSupportedInstanceExtensions{};
	std::vector<const char*> mEnabledDeviceExtensions{}; // Set of device extensions to be enabled for this example (must be set in the derived constructor)
	std::vector<const char*> mEnabledInstanceExtensions{}; // Set of instance extensions to be enabled for this example (must be set in the derived constructor)
	std::vector<VkLayerSettingEXT> mEnabledLayerSettings{}; // Set of layer settings to be enabled for this example (must be set in the derived constructor)
	std::vector<VkShaderModule> mVkShaderModules{}; // List of shader modules created (stored for cleanup)
	std::vector<VkSemaphore> mVkPresentCompleteSemaphores{};
	std::vector<VkSemaphore> mVkRenderCompleteSemaphores{};
	std::vector<float> mFrametimes{};
	std::array<VkCommandBuffer, gMaxConcurrentFrames> mVkCommandBuffers{}; // Command buffers used for rendering
	std::array<VulkanUniformBuffer, gMaxConcurrentFrames> mVulkanUniformBuffers{};
	std::array<VkFence, gMaxConcurrentFrames> mWaitVkFences{};
	std::string shaderDir;
	std::uint32_t mFramebufferWidth;
	std::uint32_t mFramebufferHeight;
	std::uint32_t mFrameCounter;
	std::uint32_t mLastFPS;
	std::uint32_t mMaxFrametimes;
	std::uint32_t mBufferIndexCount;
	std::uint32_t mCurrentFrameIndex;
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
