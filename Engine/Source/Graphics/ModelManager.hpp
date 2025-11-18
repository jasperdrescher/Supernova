#pragma once

#include "Core/Types.hpp"
#include "Math/Types.hpp"
#include "ModelFlags.hpp"
#include "UniqueIdentifier.hpp"
#include "VulkanGlTFTypes.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

struct VulkanDevice;
class TextureManager;

namespace tinygltf
{
	class Node;
	class Model;
}

class ModelManager
{
public:
	ModelManager(const std::shared_ptr<TextureManager>& aTextureManager);
	~ModelManager();

	UniqueIdentifier LoadModel(const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aTransferQueue, FileLoadingFlags aFileLoadingFlags = FileLoadingFlags::None, float aScale = 1.0f);
	vkglTF::Model* GetModel(const UniqueIdentifier aIdentifier) const;
	VkDescriptorSetLayout GetDescriptorSetLayoutImage() const { return mDescriptorSetLayoutImage; }
	VkDescriptorSetLayout GetDescriptorSetLayoutUbo() const { return mDescriptorSetLayoutUbo; }

private:
	void LoadNode(vkglTF::Model& aModel, tinygltf::Model* aGltfModel, vkglTF::Node* aParent, const tinygltf::Node* aNode, Core::uint32 aNodeIndex, std::vector<Core::uint32>& aIndexBuffer, std::vector<vkglTF::Vertex>& aVertexBuffer, float aGlobalscale);
	void LoadSkins(vkglTF::Model& aModel, tinygltf::Model* aGltfModel);
	void LoadImages(vkglTF::Model& aModel, tinygltf::Model* aGltfModel);
	void LoadMaterials(vkglTF::Model& aModel, tinygltf::Model* aGltfModel);
	void LoadAnimations(vkglTF::Model& aModel, tinygltf::Model* aGltfModel);
	void GetNodeDimensions(const vkglTF::Node* aNode, Math::Vector3f& aMin, Math::Vector3f& aMax);
	void GetSceneDimensions(vkglTF::Model& aModel);
	void UpdateAnimation(vkglTF::Model& aModel, Core::uint32 aIndex, float aTime);
	void CreateDescriptorSets(vkglTF::Model& aModel, VulkanDevice* aDevice);
	void CreateMaterialDescriptorSets(vkglTF::Material& material);
	void CreateDescriptorPool(Core::uint32 uboCount, Core::uint32 imageCount, VulkanDevice* aDevice);
	void CreateNodeDescriptorSets(vkglTF::Node* aNode, const VkDescriptorSetLayout aDescriptorSetLayout);
	void CreateBuffers(vkglTF::Model& aModel, std::vector<Core::uint32>& indexBuffer, std::vector<vkglTF::Vertex>& vertexBuffer, Core::size vertexBufferSize, Core::size indexBufferSize, VulkanDevice* aDevice, VkQueue aTransferQueue);

	vkglTF::Node* FindNode(vkglTF::Node* aParent, Core::uint32 aIndex);
	vkglTF::Node* NodeFromIndex(vkglTF::Model& aModel, Core::uint32 aIndex);
	vkglTF::Texture* GetTexture(vkglTF::Model& aModel, Core::uint32 aIndex);

	VkDescriptorSetLayout mDescriptorSetLayoutUbo;
	VkDescriptorSetLayout mDescriptorSetLayoutImage;
	VkDescriptorPool mDescriptorPool;
	DescriptorBindingFlags mDescriptorBindingFlags;
	std::weak_ptr<TextureManager> mTextureManager;
	VulkanDevice* mVulkanDevice;
	std::map<UniqueIdentifier, vkglTF::Model*> mModels;
};
