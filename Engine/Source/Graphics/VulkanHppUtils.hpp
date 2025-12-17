#pragma once

#include <vulkan/vulkan.hpp>

namespace vk
{
	namespace su
	{
		VKAPI_ATTR vk::Bool32 VKAPI_CALL debugUtilsMessengerCallback(
			vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
			vk::DebugUtilsMessengerCallbackDataEXT const* pCallbackData,
			void* /*pUserData*/);

		uint32_t findGraphicsQueueFamilyIndex(std::vector<vk::QueueFamilyProperties> const& queueFamilyProperties);
		
		vk::DebugUtilsMessengerCreateInfoEXT makeDebugUtilsMessengerCreateInfoEXT();

		std::vector<char const*> gatherLayers(std::vector<std::string> const& layers
#ifndef NDEBUG
			,
			std::vector<vk::LayerProperties> const& layerProperties
#endif
		);

		std::vector<char const*> gatherExtensions(std::vector<std::string> const& extensions
#ifndef NDEBUG
			,
			std::vector<vk::ExtensionProperties> const& extensionProperties
#endif
		);

#ifndef NDEBUG
		vk::StructureChain<vk::InstanceCreateInfo>
#else
		vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT>
#endif
			makeInstanceCreateInfoChain(vk::InstanceCreateFlagBits instanceCreateFlagBits,
				vk::ApplicationInfo const& applicationInfo,
				std::vector<char const*> const& layers,
				std::vector<char const*> const& extensions);
	}  // namespace su
}  // namespace vk
