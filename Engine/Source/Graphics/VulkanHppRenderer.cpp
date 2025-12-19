#include "VulkanHppRenderer.hpp"

#include "EngineProperties.hpp"
#include "VulkanHppUtils.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

VulkanHppRenderer::VulkanHppRenderer(const std::shared_ptr<EngineProperties>& aEngineProperties, const std::shared_ptr<Window>& aWindow)
	: mEngineProperties{aEngineProperties}
	, mWindow{aWindow}
{
	mEngineProperties.lock()->mAPIVersion = VK_API_VERSION_1_4;
	mEngineProperties.lock()->mIsValidationEnabled = true;
	mEngineProperties.lock()->mIsVSyncEnabled = true;
}

void VulkanHppRenderer::InitializeRenderer()
{
	try
	{
		InitializeInstance();

#ifndef NDEBUG
		vk::DebugUtilsMessengerEXT debugUtilsMessenger = mInstance.createDebugUtilsMessengerEXT(vk::su::makeDebugUtilsMessengerCreateInfoEXT());
#endif

		vk::PhysicalDevice physicalDevice = mInstance.enumeratePhysicalDevices().front();

		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

		auto propertyIterator = std::find_if(
			queueFamilyProperties.begin(),
			queueFamilyProperties.end(),
			[](vk::QueueFamilyProperties const& qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });
		
		size_t graphicsQueueFamilyIndex = std::distance(queueFamilyProperties.begin(), propertyIterator);
		assert(graphicsQueueFamilyIndex < queueFamilyProperties.size());

		float queuePriority = 0.5f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), static_cast<uint32_t>(graphicsQueueFamilyIndex), 1, &queuePriority);
		vk::Device device = physicalDevice.createDevice(vk::DeviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo));

		VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

#ifndef NDEBUG
		mInstance.destroyDebugUtilsMessengerEXT(debugUtilsMessenger);
#endif

		device.destroy();
		mInstance.destroy();
	}
	catch (vk::SystemError const& err)
	{
		std::cout << "vk::SystemError: " << err.what() << std::endl;
		exit(-1);
	}
	catch (...)
	{
		std::cout << "unknown error\n";
		exit(-1);
	}
}

void VulkanHppRenderer::InitializeInstance()
{
	VULKAN_HPP_DEFAULT_DISPATCHER.init();

	std::vector<std::string> layers = {};
	std::vector<char const*> enabledLayers = vk::su::gatherLayers(layers
#ifndef NDEBUG
		,
		vk::enumerateInstanceLayerProperties()
#endif
	);

	std::vector<std::string> extensions = {};
	std::vector<char const*> enabledExtensions = vk::su::gatherExtensions(extensions
#ifndef NDEBUG
		,
		vk::enumerateInstanceExtensionProperties()
#endif
	);

	vk::ApplicationInfo applicationInfo(mEngineProperties.lock()->mApplicationName.c_str(), 1, mEngineProperties.lock()->mEngineName.c_str(), 1, mEngineProperties.lock()->mAPIVersion);
	vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo);

	mInstance = vk::createInstance(vk::su::makeInstanceCreateInfoChain({}, applicationInfo, enabledLayers, enabledExtensions).get<vk::InstanceCreateInfo>());

	VULKAN_HPP_DEFAULT_DISPATCHER.init(mInstance);
}
