#pragma once

#include "vulkan/vulkan_core.h"

namespace VulkanDebug
{
	VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessageCallback(VkDebugUtilsMessageSeverityFlagBitsEXT aMessageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT aMessageType,
		const VkDebugUtilsMessengerCallbackDataEXT* aCallbackData,
		void* aUserData);

	// Load debug function pointers and set debug callback
	void SetupDebugUtilsMessenger(VkInstance aVkInstance);

	// Clear debug callback
	void DestroyDebugUtilsMessenger(VkInstance aVkInstance);

	// Used to populate a VkDebugUtilsMessengerCreateInfoEXT with our example messenger function and desired flags
	void SetupDebugingMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& aDebugUtilsMessengerCI);

	void SetupDebugUtils(VkInstance aVkInstance);
}
