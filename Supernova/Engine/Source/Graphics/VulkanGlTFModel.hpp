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

namespace vkglTF
{
	class Model
	{
	public:
		Model(TextureManager* aTextureManager);
		~Model();

		void LoadFromFile(const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aTransferQueue, std::uint32_t aFileLoadingFlags = vkglTF::FileLoadingFlags::None, float aScale = 1.0f);
		void CreateDescriptorSets(VulkanDevice* aDevice);
		void CreateMaterialDescriptorSets(vkglTF::Material& material);
		void CreateDescriptorPool(uint32_t uboCount, uint32_t imageCount, VulkanDevice* aDevice);
		void BindBuffers(VkCommandBuffer aCommandBuffer);

		Vertices vertices{};
		Indices indices{};
		std::vector<Node*> nodes;
		std::vector<Texture> textures{};
		std::vector<Material> materials{};
		bool buffersBound;

	private:
		void LoadNode(vkglTF::Node* aParent, const tinygltf::Node* aNode, std::uint32_t aNodeIndex, std::vector<std::uint32_t>& aIndexBuffer, std::vector<Vertex>& aVertexBuffer, float aGlobalscale);
		void LoadSkins();
		void LoadImages();
		void LoadMaterials();
		void LoadAnimations();
		void GetNodeDimensions(const Node* aNode, Math::Vector3f& aMin, Math::Vector3f& aMax);
		void GetSceneDimensions();
		void UpdateAnimation(std::uint32_t aIndex, float aTime);
		void CreateNodeDescriptorSets(vkglTF::Node* aNode, const VkDescriptorSetLayout aDescriptorSetLayout);
		void CreateBuffers(std::vector<std::uint32_t>& indexBuffer, std::vector<vkglTF::Vertex>& vertexBuffer, size_t vertexBufferSize, size_t indexBufferSize, VulkanDevice* aDevice, VkQueue aTransferQueue);

		Node* FindNode(Node* aParent, std::uint32_t aIndex);
		Node* NodeFromIndex(std::uint32_t aIndex);
		vkglTF::Texture* GetTexture(std::uint32_t aIndex);

		TextureManager* mTextureManager;
		vkglTF::Texture mEmptyTexture;
		VkDescriptorPool descriptorPool;
		std::vector<Node*> linearNodes;
		std::vector<Skin*> skins;
		std::vector<Animation> animations{};
		Dimensions mDimensions;
		std::filesystem::path path;
		tinygltf::Model* mCurrentModel;
		VulkanDevice* mVulkanDevice;
	};
}
