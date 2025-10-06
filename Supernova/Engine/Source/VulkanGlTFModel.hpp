#pragma once

#include "VulkanGlTFTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
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
		void Draw(VkCommandBuffer aCommandBuffer, std::uint32_t aRenderFlags = 0, VkPipelineLayout aPipelineLayout = VK_NULL_HANDLE, std::uint32_t aBindImageSet = 1);

	private:
		void LoadNode(vkglTF::Node* aParent, const tinygltf::Node* aNode, std::uint32_t aNodeIndex, std::vector<std::uint32_t>& aIndexBuffer, std::vector<Vertex>& aVertexBuffer, float aGlobalscale);
		void LoadSkins();
		void LoadImages(VulkanDevice* aDevice, VkQueue aTransferQueue);
		void LoadMaterials();
		void LoadAnimations();
		void BindBuffers(VkCommandBuffer aCommandBuffer);
		void GetNodeDimensions(const Node* aNode, glm::vec3& aMin, glm::vec3& aMax);
		void GetSceneDimensions();
		void UpdateAnimation(std::uint32_t aIndex, float aTime);
		void PrepareNodeDescriptor(vkglTF::Node* aNode, VkDescriptorSetLayout aDescriptorSetLayout);
		void DrawNode(const Node* aNode, VkCommandBuffer aCommandBuffer, std::uint32_t aRenderFlags = 0, VkPipelineLayout aPipelineLayout = VK_NULL_HANDLE, std::uint32_t aBindImageSet = 1);
		void CreateEmptyTexture(VkQueue aTransferQueue);

		Node* FindNode(Node* aParent, std::uint32_t aIndex);
		Node* NodeFromIndex(std::uint32_t aIndex);
		vkglTF::Texture* GetTexture(std::uint32_t aIndex);

		vkglTF::Texture mEmptyTexture;
		VkDescriptorPool descriptorPool;
		std::vector<Node*> nodes;
		std::vector<Node*> linearNodes;
		std::vector<Skin*> skins;
		std::vector<Texture> textures{};
		std::vector<Material> materials{};
		std::vector<Animation> animations{};
		Vertices vertices{};
		Indices indices{};
		Dimensions mDimensions;
		std::string path;
		tinygltf::Model* mCurrentModel;
		VulkanDevice* mVulkanDevice;
		bool metallicRoughnessWorkflow;
		bool buffersBound;
	};
}
