#pragma once

#include "Math/Types.hpp"
#include "VulkanGlTFTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

struct VulkanDevice;

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
		Model();
		~Model();

		void LoadFromFile(const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aTransferQueue, std::uint32_t aFileLoadingFlags = vkglTF::FileLoadingFlags::None, float aScale = 1.0f);
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
		void LoadImages(VulkanDevice* aDevice, VkQueue aTransferQueue);
		void LoadMaterials();
		void LoadAnimations();
		void GetNodeDimensions(const Node* aNode, Math::Vector3f& aMin, Math::Vector3f& aMax);
		void GetSceneDimensions();
		void UpdateAnimation(std::uint32_t aIndex, float aTime);
		void PrepareNodeDescriptor(vkglTF::Node* aNode, VkDescriptorSetLayout aDescriptorSetLayout);
		void CreateEmptyTexture(VkQueue aTransferQueue);

		Node* FindNode(Node* aParent, std::uint32_t aIndex);
		Node* NodeFromIndex(std::uint32_t aIndex);
		vkglTF::Texture* GetTexture(std::uint32_t aIndex);

		vkglTF::Texture mEmptyTexture;
		VkDescriptorPool descriptorPool;
		std::vector<Node*> linearNodes;
		std::vector<Skin*> skins;
		std::vector<Animation> animations{};
		Dimensions mDimensions;
		std::string path;
		tinygltf::Model* mCurrentModel;
		VulkanDevice* mVulkanDevice;
		bool metallicRoughnessWorkflow;
	};
}
