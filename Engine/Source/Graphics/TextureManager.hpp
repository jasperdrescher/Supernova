#pragma once

#include "Graphics/VulkanGlTFTypes.hpp"

#include <filesystem>
#include <vulkan/vulkan_core.h>

struct VulkanCDevice;

class VulkanCTextureManager
{
public:
	VulkanCTextureManager();
	~VulkanCTextureManager();

	void SetContext(VulkanCDevice* aDevice, VkQueue aTransferQueue);

	[[nodiscard]] vkglTF::Texture CreateEmptyTexture();
	[[nodiscard]] vkglTF::Texture CreateTexture(const std::filesystem::path& aPath);
	[[nodiscard]] vkglTF::Texture CreateTexture(const std::filesystem::path& aPath, vkglTF::Image& aImage);

private:
	void CreateFromKtxTexture(const std::filesystem::path& aPath, vkglTF::Texture& aTexture, VkFormat& aFormat);
	void CreateFromEmbeddedTexture(vkglTF::Image& aImage, vkglTF::Texture& aTexture, VkFormat& aFormat);
	void CreateResources(vkglTF::Texture& aTexture, const VkFormat& aFormat);

	VulkanCDevice* mVulkanDevice;
	VkQueue mTransferQueue;
};
