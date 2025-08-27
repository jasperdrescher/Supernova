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
			bool mIsLeftDown = false;
			bool mIsRightDown = false;
			bool mIsMiddleDown = false;
		};

		Buttons mButtons;
		glm::vec2 mPosition;
	};

	void GetEnabledFeatures() const;
	void mouseMoved(double x, double y, bool& handled) {}
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
	bool initVulkan();

	/** @brief (Virtual) Creates the application wide Vulkan instance */
	VkResult createInstance();

	/** @brief Loads a SPIR-V shader file for the given shader stage */
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);

	void OnResizeWindow();

	// Returns the path to the root of the glsl, hlsl or slang shader directory.
	std::string getShadersPath() const;

	MouseState mMouseState;

	VkClearColorValue mDefaultClearColor = {{0.025f, 0.025f, 0.025f, 1.0f}};

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float mTimer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float TimerSpeed = 0.25f;
	bool mIsPaused = false;

	Camera mCamera;

	VulkanDepthStencil mVulkanDepthStencil;

	bool mIsPrepared = false;
	bool mIsResized = false;
	std::uint32_t mFramebufferWidth;
	std::uint32_t mFramebufferHeight;

	/** @brief Last frame time measured using a high performance timer (if available) */
	float mFrameTime = 1.0f;

	/** @brief Encapsulated physical and logical vulkan device */
	vks::VulkanDevice* vulkanDevice;

	// Frame counter to display fps
	std::uint32_t mFrameCounter = 0;
	std::uint32_t mLastFPS = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> mLastTimestamp;
	std::chrono::time_point<std::chrono::high_resolution_clock> mPreviousEndTime;
	// Vulkan instance, stores all per-application states
	VkInstance mVkInstance{VK_NULL_HANDLE};
	std::vector<std::string> mSupportedInstanceExtensions;
	// Physical device (GPU) that Vulkan will use
	VkPhysicalDevice mVkPhysicalDevice{VK_NULL_HANDLE};
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
	VkDevice mVkLogicalDevice{VK_NULL_HANDLE};
	// Handle to the device graphics queue that command buffers are submitted to
	VkQueue mVkQueue{VK_NULL_HANDLE};
	// Depth buffer format (selected during Vulkan initialization)
	VkFormat mVkDepthFormat{VK_FORMAT_UNDEFINED};
	// Command buffer pool
	VkCommandPool mVkCommandPool{VK_NULL_HANDLE};
	// Command buffers used for rendering
	std::array<VkCommandBuffer, gMaxConcurrentFrames> mVkCommandBuffers;
	// Descriptor set pool
	VkDescriptorPool mVkDescriptorPool{VK_NULL_HANDLE};
	// List of shader modules created (stored for cleanup)
	std::vector<VkShaderModule> mVkShaderModules;
	// Pipeline cache object
	VkPipelineCache mVkPipelineCache{VK_NULL_HANDLE};
	// Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanSwapChain mVulkanSwapChain;

	void CreateGlfwWindow();
	void SetWindowSize(int aWidth, int aHeight);

	static void KeyCallback(GLFWwindow* aWindow, int aKey, int aScancode, int aAction, int aMode);
	static void FramebufferResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
	static void WindowResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
	static void WindowMinimizedCallback(GLFWwindow* aWindow, int aValue);

	GLFWwindow* mGLFWWindow;
	bool mShouldClose;
	bool mIsFramebufferResized;
	VulkanApplicationProperties mVulkanApplicationProperties;

	VulkanBuffer mVulkanVertexBuffer;
	VulkanBuffer mVulkanIndexBuffer;
	std::uint32_t mBufferIndexCount{0};

	std::array<VulkanUniformBuffer, gMaxConcurrentFrames> mVulkanUniformBuffers;

	VkPipelineLayout mVkPipelineLayout{VK_NULL_HANDLE};
	VkPipeline mVkPipeline{VK_NULL_HANDLE};
	VkDescriptorSetLayout mVkDescriptionSetLayout{VK_NULL_HANDLE};
	std::vector<VkSemaphore> mVkPresentCompleteSemaphores{};
	std::vector<VkSemaphore> mVkRenderCompleteSemaphores{};
	std::array<VkFence, gMaxConcurrentFrames> mVkWaitFences{};

	std::uint32_t mCurrentFrameIndex{0};

	VkPhysicalDeviceVulkan13Features mVkPhysicalDevice13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

	std::string GetWindowTitle(float aDeltaTime) const;
	void handleMouseMove(std::int32_t x, std::int32_t y);
	void NextFrame();
	void CreatePipelineCache();
	void InitializeSwapchain();
	void CreateCommandPool();
	void SetupSwapchain();
	void destroyCommandBuffers();
	std::string shaderDir = "GLSL";
};
