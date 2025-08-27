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

class VulkanExample
{
public:
	VulkanBuffer vertexBuffer;
	VulkanBuffer indexBuffer;
	std::uint32_t indexCount{0};

	std::array<VulkanUniformBuffer, gMaxConcurrentFrames> uniformBuffers;

	VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
	VkPipeline pipeline{VK_NULL_HANDLE};
	VkDescriptorSetLayout descriptorSetLayout{VK_NULL_HANDLE};
	std::vector<VkSemaphore> presentCompleteSemaphores{};
	std::vector<VkSemaphore> renderCompleteSemaphores{};
	std::array<VkFence, gMaxConcurrentFrames> waitFences{};

	VkCommandPool commandPool{VK_NULL_HANDLE};
	std::array<VkCommandBuffer, gMaxConcurrentFrames> commandBuffers{};

	std::uint32_t currentFrame{0};

	VkPhysicalDeviceVulkan13Features mVkPhysicalDevice13Features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

	VulkanExample();
	~VulkanExample();

	void InitializeRenderer();
	void PrepareUpdate();
	void UpdateRenderer(float aDeltaTime);
	void DestroyRenderer();

	bool ShouldClose() const { return mShouldClose; }
	bool IsPaused() const { return mIsPaused; }

	void getEnabledFeatures();
	void mouseMoved(double x, double y, bool& handled) {}
	void windowResized() {}
	void prepare();
	void PrepareFrame();
	void setupDepthStencil();

	std::uint32_t getMemoryTypeIndex(std::uint32_t typeBits, VkMemoryPropertyFlags properties);
	void createSynchronizationPrimitives();
	void createCommandBuffers();
	void createVertexBuffer();
	void createDescriptors();
	VkShaderModule loadSPIRVShader(const std::string& filename);
	void createPipeline();
	void createUniformBuffers();

public:
	bool prepared = false;
	bool resized = false;
	std::uint32_t width = 1280;
	std::uint32_t height = 720;

	/** @brief Last frame time measured using a high performance timer (if available) */
	float mFrameTime = 1.0f;

	/** @brief Encapsulated physical and logical vulkan device */
	vks::VulkanDevice* vulkanDevice;

	/** @brief Example settings that can be changed e.g. by command line arguments */
	struct Settings
	{
		/** @brief Activates validation layers (and message output) when set to true */
		bool validation = false;
		/** @brief Set to true if fullscreen mode has been requested via command line */
		bool fullscreen = false;
		/** @brief Set to true if v-sync will be forced for the swapchain */
		bool vsync = false;
	} settings;

	/** @brief State of mouse/touch input */
	struct
	{
		struct
		{
			bool left = false;
			bool right = false;
			bool middle = false;
		} buttons;
		glm::vec2 position;
	} mouseState;

	VkClearColorValue mDefaultClearColor = {{0.025f, 0.025f, 0.025f, 1.0f}};

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float mTimer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float TimerSpeed = 0.25f;
	bool mIsPaused = false;

	Camera camera;

	std::string title;
	std::string name;
	std::uint32_t apiVersion;

	VulkanDepthStencil depthStencil;

	/** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
	bool initVulkan();

	/** @brief (Virtual) Creates the application wide Vulkan instance */
	VkResult createInstance();

	/** @brief Loads a SPIR-V shader file for the given shader stage */
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);

	void windowResize();

protected:
	// Returns the path to the root of the glsl, hlsl or slang shader directory.
	std::string getShadersPath() const;

	// Frame counter to display fps
	std::uint32_t mFrameCounter = 0;
	std::uint32_t mLastFPS = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> mLastTimestamp;
	std::chrono::time_point<std::chrono::high_resolution_clock> mPreviousEndTime;
	// Vulkan instance, stores all per-application states
	VkInstance mVkInstance{VK_NULL_HANDLE};
	std::vector<std::string> supportedInstanceExtensions;
	// Physical device (GPU) that Vulkan will use
	VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
	// Stores physical device properties (for e.g. checking device limits)
	VkPhysicalDeviceProperties deviceProperties{};
	// Stores the features available on the selected physical device (for e.g. checking if a feature is available)
	VkPhysicalDeviceFeatures mEnabledVkPhysicalDeviceFeatures{};
	// Stores all available memory (type) properties for the physical device
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties{};
	/** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> enabledDeviceExtensions;
	/** @brief Set of instance extensions to be enabled for this example (must be set in the derived constructor) */
	std::vector<const char*> enabledInstanceExtensions;
	/** @brief Set of layer settings to be enabled for this example (must be set in the derived constructor) */
	std::vector<VkLayerSettingEXT> enabledLayerSettings;
	/** @brief Logical device, application's view of the physical device (GPU) */
	VkDevice mVkLogicalDevice{VK_NULL_HANDLE};
	// Handle to the device graphics queue that command buffers are submitted to
	VkQueue queue{VK_NULL_HANDLE};
	// Depth buffer format (selected during Vulkan initialization)
	VkFormat depthFormat{VK_FORMAT_UNDEFINED};
	// Command buffer pool
	VkCommandPool mVkCommandPool{VK_NULL_HANDLE};
	// Command buffers used for rendering
	std::array<VkCommandBuffer, gMaxConcurrentFrames> mVkCommandBuffers;
	// Global render pass for frame buffer writes
	VkRenderPass renderPass{VK_NULL_HANDLE};
	// Descriptor set pool
	VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
	// List of shader modules created (stored for cleanup)
	std::vector<VkShaderModule> shaderModules;
	// Pipeline cache object
	VkPipelineCache pipelineCache{VK_NULL_HANDLE};
	// Wraps the swap chain to present images (framebuffers) to the windowing system
	VulkanSwapChain mVulkanSwapChain;

	// Synchronization related objects and variables
	// These are used to have multiple frame buffers "in flight" to get some CPU/GPU parallelism
	std::uint32_t currentImageIndex{0};
	std::uint32_t currentBuffer{0};

	bool requiresStencil{false};

private:
	void CreateGlfwWindow();

	GLFWwindow* mGlfwWindow;
	bool mShouldClose;

	std::string GetWindowTitle(float aDeltaTime) const;
	std::uint32_t destWidth;
	std::uint32_t destHeight;
	void handleMouseMove(int32_t x, int32_t y);
	void NextFrame();
	void createPipelineCache();
	void InitializeSwapchain();
	void createCommandPool();
	void SetupSwapchain();
	void destroyCommandBuffers();
	std::string shaderDir = "GLSL";
};
