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
	struct MouseState
	{
		struct Buttons
		{
			Buttons() : mIsLeftDown{false}, mIsRightDown{false}, mIsMiddleDown{false} {}

			bool mIsLeftDown;
			bool mIsRightDown;
			bool mIsMiddleDown;
		};

		Buttons mButtons;
		glm::vec2 mPosition;
	};

	void GetEnabledFeatures() const;
	void PrepareVulkanResources();
	void PrepareFrame();
	void SetupDepthStencil();

	std::uint32_t GetMemoryTypeIndex(std::uint32_t typeBits, VkMemoryPropertyFlags properties) const;
	void CreateSynchronizationPrimitives();
	void CreateCommandBuffers();
	void CreateVertexBuffer();
	void CreateDescriptors();
	VkShaderModule LoadSPIRVShader(const std::string& filename) const;
	void CreatePipeline();
	void CreateUniformBuffers();

	/** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
	bool InitializeVulkan();

	VkResult CreateVkInstance();
	VkResult CreateVulkanDevice();

	/** @brief Loads a SPIR-V shader file for the given shader stage */
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);

	void OnResizeWindow();

	// Returns the path to the root of the glsl, hlsl or slang shader directory.
	std::string getShadersPath() const;

	void CreateGlfwWindow();
	void SetWindowSize(int aWidth, int aHeight);

	std::string GetWindowTitle(float aDeltaTime) const;
	void handleMouseMove(std::int32_t x, std::int32_t y);
	void NextFrame();
	void CreatePipelineCache();
	void InitializeSwapchain();
	void CreateCommandPool();
	void SetupSwapchain();
	void destroyCommandBuffers();

	static void KeyCallback(GLFWwindow* aWindow, int aKey, int aScancode, int aAction, int aMode);
	static void FramebufferResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
	static void WindowResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
	static void WindowMinimizedCallback(GLFWwindow* aWindow, int aValue);

	MouseState mMouseState;

	VkClearColorValue mDefaultClearColor;

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float mTimer;
	// Multiplier for speeding up (or slowing down) the global timer
	float TimerSpeed;
	bool mIsPaused;

	Camera mCamera;

	VulkanDepthStencil mVulkanDepthStencil;

	bool mIsPrepared;
	bool mIsResized;
	std::uint32_t mFramebufferWidth;
	std::uint32_t mFramebufferHeight;

	/** @brief Last frame time measured using a high performance timer (if available) */
	float mFrameTime;

	/** @brief Encapsulated physical and logical vulkan device */
	VulkanDevice* mVulkanDevice;

	// Frame counter to display fps
	std::uint32_t mFrameCounter = 0;
	std::uint32_t mLastFPS = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> mLastTimestamp;
	std::chrono::time_point<std::chrono::high_resolution_clock> mPreviousEndTime;
	// Vulkan instance, stores all per-application states
	VkInstance mVkInstance;
	std::vector<std::string> mSupportedInstanceExtensions;
	// Physical device (GPU) that Vulkan will use
	VkPhysicalDevice mVkPhysicalDevice;
	// Stores physical device properties (for e.g. checking device limits)
	VkPhysicalDeviceProperties mVkPhysicalDeviceProperties{};
	// Stores the features available on the selected physical device (for e.g. checking if a feature is available)
	VkPhysicalDeviceFeatures mEnabledVkPhysicalDeviceFeatures{};
	// Stores all available memory (type) properties for the physical device
	VkPhysicalDeviceMemoryProperties mVkPhysicalDeviceMemoryProperties{};
	/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> mEnabledDeviceExtensions;
	/** @brief Set of instance extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> mEnabledInstanceExtensions;
	/** @brief Set of layer settings to be enabled for this example (must be set in the derived constructor) */
	std::vector<VkLayerSettingEXT> mEnabledLayerSettings;
	/** @brief Logical device, application's view of the physical device (GPU) */
	VkDevice mVkLogicalDevice;
	// Handle to the device graphics queue that command buffers are submitted to
	VkQueue mVkQueue;
	// Depth buffer format (selected during Vulkan initialization)
	VkFormat mVkDepthFormat;
	// Command buffer pool
	VkCommandPool mVkCommandPool;
	// Command buffers used for rendering
	std::array<VkCommandBuffer, gMaxConcurrentFrames> mVkCommandBuffers;
	// Descriptor set pool
	VkDescriptorPool mVkDescriptorPool;
	// List of shader modules created (stored for cleanup)
	std::vector<VkShaderModule> mVkShaderModules;
	// Pipeline cache object
	VkPipelineCache mVkPipelineCache;
	// Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanSwapChain mVulkanSwapChain;

	GLFWwindow* mGLFWWindow;
	bool mShouldClose;
	bool mIsFramebufferResized;
	VulkanApplicationProperties mVulkanApplicationProperties;

	VulkanBuffer mVulkanVertexBuffer;
	VulkanBuffer mVulkanIndexBuffer;
	std::uint32_t mBufferIndexCount;

	std::array<VulkanUniformBuffer, gMaxConcurrentFrames> mVulkanUniformBuffers;

	VkPipelineLayout mVkPipelineLayout;
	VkPipeline mVkPipeline;
	VkDescriptorSetLayout mVkDescriptionSetLayout;
	std::vector<VkSemaphore> mVkPresentCompleteSemaphores{};
	std::vector<VkSemaphore> mVkRenderCompleteSemaphores{};
	std::array<VkFence, gMaxConcurrentFrames> mVkWaitFences{};

	std::uint32_t mCurrentFrameIndex;

	VkPhysicalDeviceVulkan13Features mVkPhysicalDevice13Features;

	std::string shaderDir;
};
