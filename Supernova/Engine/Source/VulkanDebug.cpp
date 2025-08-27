#include "VulkanDebug.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

namespace vks
{
	namespace debug
	{
		PFN_vkCreateDebugUtilsMessengerEXT gVkCreateDebugUtilsMessengerFunction;
		PFN_vkDestroyDebugUtilsMessengerEXT gVkDestroyDebugUtilsMessengerFunction;
		VkDebugUtilsMessengerEXT gVkDebugUtilsMessenger;

		VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessageCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT aMessageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT aMessageType,
			const VkDebugUtilsMessengerCallbackDataEXT* aCallbackData,
			void* aUserData)
		{
			// Select prefix depending on flags passed to the callback
			std::string prefix;

			if (aMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
			{
				prefix = "VERBOSE: ";
#if defined(_WIN32)
				prefix = "\033[32m" + prefix + "\033[0m";
#endif
			}
			else if (aMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
			{
				prefix = "INFO: ";
#if defined(_WIN32)
				prefix = "\033[36m" + prefix + "\033[0m";
#endif
			}
			else if (aMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			{
				prefix = "WARNING: ";
#if defined(_WIN32)
				prefix = "\033[33m" + prefix + "\033[0m";
#endif
			}
			else if (aMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			{
				prefix = "ERROR: ";
#if defined(_WIN32)
				prefix = "\033[31m" + prefix + "\033[0m";
#endif
			}

			std::stringstream debugMessage;
			if (aCallbackData->pMessageIdName)
			{
				debugMessage << prefix << "[" << aCallbackData->messageIdNumber << "][" << aCallbackData->pMessageIdName << "] : " << aCallbackData->pMessage;
			}
			else
			{
				debugMessage << prefix << "[" << aCallbackData->messageIdNumber << "] : " << aCallbackData->pMessage;
			}

			if (aMessageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			{
				std::cerr << debugMessage.str() << std::endl;
			}
			else
			{
				std::cout << debugMessage.str() << std::endl;
			}

			// The return value of this callback controls whether the Vulkan call that caused the validation message will be aborted or not
			// We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message to abort
			// If you instead want to have calls abort, pass in VK_TRUE and the function will return VK_ERROR_VALIDATION_FAILED_EXT
			return VK_FALSE;
		}

		void SetupDebugingMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& aVkDebugUtilsMessengerCreateInfo)
		{
			aVkDebugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			aVkDebugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			aVkDebugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			aVkDebugUtilsMessengerCreateInfo.pfnUserCallback = DebugUtilsMessageCallback;
		}

		void SetupDebugUtilsMessenger(VkInstance aVkInstance)
		{
			gVkCreateDebugUtilsMessengerFunction = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(aVkInstance, "vkCreateDebugUtilsMessengerEXT"));
			gVkDestroyDebugUtilsMessengerFunction = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(aVkInstance, "vkDestroyDebugUtilsMessengerEXT"));

			VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
			SetupDebugingMessengerCreateInfo(debugUtilsMessengerCI);
			VkResult result = gVkCreateDebugUtilsMessengerFunction(aVkInstance, &debugUtilsMessengerCI, nullptr, &gVkDebugUtilsMessenger);
			assert(result == VK_SUCCESS);
		}

		void DestroyDebugUtilsMessenger(VkInstance aVkInstance)
		{
			if (gVkDebugUtilsMessenger != VK_NULL_HANDLE)
			{
				gVkDestroyDebugUtilsMessengerFunction(aVkInstance, gVkDebugUtilsMessenger, nullptr);
			}
		}
	}

	namespace debugutils
	{
		PFN_vkCmdBeginDebugUtilsLabelEXT gVkCmdBeginDebugUtilsLabelFunction{nullptr};
		PFN_vkCmdEndDebugUtilsLabelEXT gVkCmdEndDebugUtilsLabelFunction{nullptr};
		PFN_vkCmdInsertDebugUtilsLabelEXT gVkCmdInsertDebugUtilsLabelFunction{nullptr};

		void SetupDebugUtils(VkInstance aVkInstance)
		{
			gVkCmdBeginDebugUtilsLabelFunction = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetInstanceProcAddr(aVkInstance, "vkCmdBeginDebugUtilsLabelEXT"));
			gVkCmdEndDebugUtilsLabelFunction = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetInstanceProcAddr(aVkInstance, "vkCmdEndDebugUtilsLabelEXT"));
			gVkCmdInsertDebugUtilsLabelFunction = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(vkGetInstanceProcAddr(aVkInstance, "vkCmdInsertDebugUtilsLabelEXT"));
		}
	};
}
