#include "VulkanHppRenderer.hpp"

#include "EngineProperties.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
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
		vk::detail::defaultDispatchLoaderDynamic.init();

		vk::Instance instance = vk::createInstance({}, nullptr);

		// initialize function pointers for instance
		vk::detail::defaultDispatchLoaderDynamic.init(instance);

		// create a dispatcher, based on additional vkDevice/vkGetDeviceProcAddr
		std::vector<vk::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
		assert(!physicalDevices.empty());

		vk::Device device = physicalDevices[0].createDevice({}, nullptr);

		// optional function pointer specialization for device
		vk::detail::defaultDispatchLoaderDynamic.init(device);
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
