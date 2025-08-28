#pragma once

#include "vulkan/vulkan_core.h"

namespace VulkanDebug
{
	VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessageCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	// Load debug function pointers and set debug callback
	void SetupDebugUtilsMessenger(VkInstance instance);
	// Clear debug callback
	void DestroyDebugUtilsMessenger(VkInstance instance);
	// Used to populate a VkDebugUtilsMessengerCreateInfoEXT with our example messenger function and desired flags
	void SetupDebugingMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugUtilsMessengerCI);

	void SetupDebugUtils(VkInstance instance);
}
