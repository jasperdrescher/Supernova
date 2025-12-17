#include "VulkanHppUtils.hpp"

#include <iostream>
#include <vulkan/vulkan.hpp>

namespace vk
{
	namespace su
	{
		VKAPI_ATTR vk::Bool32 VKAPI_CALL debugUtilsMessengerCallback(
			vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
			const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* /*pUserData*/)
		{
#ifndef NDEBUG
			switch (static_cast<uint32_t>(pCallbackData->messageIdNumber))
			{
				case 0:
					// Validation Warning: Override layer has override paths set to C:/VulkanSDK/<version>/Bin
					return vk::False;
				case 0x822806fa:
					// Validation Warning: vkCreateInstance(): to enable extension VK_EXT_debug_utils, but this extension is intended to support use by applications when
					// debugging and it is strongly recommended that it be otherwise avoided.
					return vk::False;
				case 0xe8d1a9fe:
					// Validation Performance Warning: Using debug builds of the validation layers *will* adversely affect performance.
					return vk::False;
			}
#endif

			std::cerr << vk::to_string(messageSeverity) << ": " << vk::to_string(messageTypes) << ":\n";
			std::cerr << std::string("\t") << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
			std::cerr << std::string("\t") << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
			std::cerr << std::string("\t") << "message         = <" << pCallbackData->pMessage << ">\n";
			if (0 < pCallbackData->queueLabelCount)
			{
				std::cerr << std::string("\t") << "Queue Labels:\n";
				for (uint32_t i = 0; i < pCallbackData->queueLabelCount; i++)
				{
					std::cerr << std::string("\t\t") << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
				}
			}
			if (0 < pCallbackData->cmdBufLabelCount)
			{
				std::cerr << std::string("\t") << "CommandBuffer Labels:\n";
				for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
				{
					std::cerr << std::string("\t\t") << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
				}
			}
			if (0 < pCallbackData->objectCount)
			{
				std::cerr << std::string("\t") << "Objects:\n";
				for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
				{
					std::cerr << std::string("\t\t") << "Object " << i << "\n";
					std::cerr << std::string("\t\t\t") << "objectType   = " << vk::to_string(pCallbackData->pObjects[i].objectType) << "\n";
					std::cerr << std::string("\t\t\t") << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
					if (pCallbackData->pObjects[i].pObjectName)
					{
						std::cerr << std::string("\t\t\t") << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
					}
				}
			}
			return vk::False;
		}

		uint32_t findGraphicsQueueFamilyIndex(std::vector<vk::QueueFamilyProperties> const& queueFamilyProperties)
		{
			// get the first index into queueFamiliyProperties which supports graphics
			std::vector<vk::QueueFamilyProperties>::const_iterator graphicsQueueFamilyProperty =
				std::find_if(queueFamilyProperties.begin(),
					queueFamilyProperties.end(),
					[](vk::QueueFamilyProperties const& qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });
			assert(graphicsQueueFamilyProperty != queueFamilyProperties.end());
			return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
		}

		vk::DebugUtilsMessengerCreateInfoEXT makeDebugUtilsMessengerCreateInfoEXT()
		{
			return {{},
					 vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
					 vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
					   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
					 &vk::su::debugUtilsMessengerCallback};
		}

		std::vector<char const*> gatherLayers(std::vector<std::string> const& layers
#ifndef NDEBUG
			,
			std::vector<vk::LayerProperties> const& layerProperties
#endif
		)
		{
			std::vector<char const*> enabledLayers;
			enabledLayers.reserve(layers.size());
			for (auto const& layer : layers)
			{
				assert(std::any_of(layerProperties.begin(), layerProperties.end(), [layer](vk::LayerProperties const& lp) { return layer == lp.layerName; }));
				enabledLayers.push_back(layer.data());
			}
#ifndef NDEBUG
			// Enable standard validation layer to find as much errors as possible!
			if (std::none_of(layers.begin(), layers.end(), [](std::string const& layer) { return layer == "VK_LAYER_KHRONOS_validation"; }) &&
				std::any_of(layerProperties.begin(),
					layerProperties.end(),
					[](vk::LayerProperties const& lp) { return (strcmp("VK_LAYER_KHRONOS_validation", lp.layerName) == 0); }))
			{
				enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
			}
#endif
			return enabledLayers;
		}

		std::vector<char const*> gatherExtensions(std::vector<std::string> const& extensions
#ifndef NDEBUG
			,
			std::vector<vk::ExtensionProperties> const& extensionProperties
#endif
		)
		{
			std::vector<char const*> enabledExtensions;
			enabledExtensions.reserve(extensions.size());
			for (auto const& ext : extensions)
			{
				assert(std::any_of(
					extensionProperties.begin(), extensionProperties.end(), [ext](vk::ExtensionProperties const& ep) { return ext == ep.extensionName; }));
				enabledExtensions.push_back(ext.data());
			}
#ifndef NDEBUG
			if (std::none_of(
				extensions.begin(), extensions.end(), [](std::string const& extension) { return extension == VK_EXT_DEBUG_UTILS_EXTENSION_NAME; }) &&
				std::any_of(extensionProperties.begin(),
					extensionProperties.end(),
					[](vk::ExtensionProperties const& ep) { return (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, ep.extensionName) == 0); }))
			{
				enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}
#endif
			return enabledExtensions;
		}

#ifndef NDEBUG
		vk::StructureChain<vk::InstanceCreateInfo>
#else
		vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT>
#endif
			makeInstanceCreateInfoChain(vk::InstanceCreateFlagBits        instanceCreateFlagBits,
				vk::ApplicationInfo const& applicationInfo,
				std::vector<char const*> const& layers,
				std::vector<char const*> const& extensions)
		{
#ifndef NDEBUG
			// in non-debug mode just use the InstanceCreateInfo for instance creation
			vk::StructureChain<vk::InstanceCreateInfo> instanceCreateInfo({instanceCreateFlagBits, &applicationInfo, layers, extensions});
#else
			// in debug mode, addionally use the debugUtilsMessengerCallback in instance creation!
			vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
			vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
				vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
			vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> instanceCreateInfo(
				{instanceCreateFlagBits, &applicationInfo, layers, extensions}, {{}, severityFlags, messageTypeFlags, &vk::su::debugUtilsMessengerCallback});
#endif
			return instanceCreateInfo;
		}
	}  // namespace su
}  // namespace vk
