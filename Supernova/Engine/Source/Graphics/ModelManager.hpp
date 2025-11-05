#pragma once

#include "Math/Types.hpp"
#include "VulkanGlTFTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>
#include <vulkan/vulkan_core.h>

struct VulkanDevice;
class TextureManager;

namespace tinygltf
{
	class Node;
	class Model;
}

enum FileLoadingFlags
{
	None = 0x00000000,
	PreTransformVertices = 0x00000001,
	PreMultiplyVertexColors = 0x00000002,
	FlipY = 0x00000004,
	DontLoadImages = 0x00000008
};

enum RenderFlags
{
	BindImages = 0x00000001,
	RenderOpaqueNodes = 0x00000002,
	RenderAlphaMaskedNodes = 0x00000004,
	RenderAlphaBlendedNodes = 0x00000008
};

class ModelManager
{
public:
	ModelManager(TextureManager* aTextureManager);
	~ModelManager();

	vkglTF::Model* LoadFromFile(const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aTransferQueue, std::uint32_t aFileLoadingFlags = FileLoadingFlags::None, float aScale = 1.0f);
	void BindBuffers(vkglTF::Model& aModel, VkCommandBuffer aCommandBuffer);

private:
	void LoadNode(vkglTF::Model& aModel, tinygltf::Model* aGltfModel, vkglTF::Node* aParent, const tinygltf::Node* aNode, std::uint32_t aNodeIndex, std::vector<std::uint32_t>& aIndexBuffer, std::vector<vkglTF::Vertex>& aVertexBuffer, float aGlobalscale);
	void LoadSkins(vkglTF::Model& aModel, tinygltf::Model* aGltfModel);
	void LoadImages(vkglTF::Model& aModel, tinygltf::Model* aGltfModel);
	void LoadMaterials(vkglTF::Model& aModel, tinygltf::Model* aGltfModel);
	void LoadAnimations(vkglTF::Model& aModel, tinygltf::Model* aGltfModel);
	void GetNodeDimensions(const vkglTF::Node* aNode, Math::Vector3f& aMin, Math::Vector3f& aMax);
	void GetSceneDimensions(vkglTF::Model& aModel);
	void UpdateAnimation(vkglTF::Model& aModel, std::uint32_t aIndex, float aTime);
	void CreateDescriptorSets(vkglTF::Model& aModel, VulkanDevice* aDevice);
	void CreateMaterialDescriptorSets(vkglTF::Material& material);
	void CreateDescriptorPool(uint32_t uboCount, uint32_t imageCount, VulkanDevice* aDevice);
	void CreateNodeDescriptorSets(vkglTF::Node* aNode, const VkDescriptorSetLayout aDescriptorSetLayout);
	void CreateBuffers(vkglTF::Model& aModel, std::vector<std::uint32_t>& indexBuffer, std::vector<vkglTF::Vertex>& vertexBuffer, size_t vertexBufferSize, size_t indexBufferSize, VulkanDevice* aDevice, VkQueue aTransferQueue);

	vkglTF::Node* FindNode(vkglTF::Node* aParent, std::uint32_t aIndex);
	vkglTF::Node* NodeFromIndex(vkglTF::Model& aModel, std::uint32_t aIndex);
	vkglTF::Texture* GetTexture(vkglTF::Model& aModel, std::uint32_t aIndex);

	TextureManager* mTextureManager;
	VulkanDevice* mVulkanDevice;
	VkDescriptorPool descriptorPool;
	std::vector<vkglTF::Model> mModels;
};
