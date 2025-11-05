#pragma once

#include "Graphics/VulkanGlTFTypes.hpp"

#include <filesystem>
#include <vulkan/vulkan_core.h>

struct VulkanDevice;

class TextureManager
{
public:
	TextureManager();
	~TextureManager();

	void SetContext(VulkanDevice* aDevice, VkQueue aTransferQueue);

	[[nodiscard]] vkglTF::Texture CreateEmptyTexture();
	[[nodiscard]] vkglTF::Texture CreateTexture(const std::filesystem::path& aPath, vkglTF::Image& aImage);

private:
	void CreateFromKtxTexture(const std::filesystem::path& aPath, vkglTF::Texture& aTexture, VkFormat& aFormat);
	void CreateFromIncludedTexture(vkglTF::Image& aImage, vkglTF::Texture& aTexture, VkFormat& aFormat);

	VulkanDevice* mVulkanDevice;
	VkQueue mTransferQueue;
};
