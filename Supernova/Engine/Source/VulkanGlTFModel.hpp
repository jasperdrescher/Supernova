#pragma once

#include "VulkanDevice.hpp"
#include "VulkanGlTFTypes.hpp"

#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vulkan/vulkan_core.h>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <cfloat>

namespace vkglTF
{
	class Model
	{
	public:
		Model() {};
		~Model();

		void LoadFromFile(const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aTransferQueue, std::uint32_t aFileLoadingFlags = vkglTF::FileLoadingFlags::None, float aScale = 1.0f);
		void Draw(VkCommandBuffer aCommandBuffer, std::uint32_t aRenderFlags = 0, VkPipelineLayout aPipelineLayout = VK_NULL_HANDLE, std::uint32_t aBindImageSet = 1);

	private:
		void LoadNode(vkglTF::Node* aParent, const tinygltf::Node& aNode, std::uint32_t aNodeIndex, const tinygltf::Model& aModel, std::vector<std::uint32_t>& aIndexBuffer, std::vector<Vertex>& aVertexBuffer, float aGlobalscale);
		void LoadSkins(tinygltf::Model& aGlTFModel);
		void LoadImages(tinygltf::Model& aGlTFModel, VulkanDevice* aDevice, VkQueue aTransferQueue);
		void LoadMaterials(tinygltf::Model& aGlTFModel);
		void LoadAnimations(tinygltf::Model& aGlTFModel);
		void BindBuffers(VkCommandBuffer aCommandBuffer);
		void GetNodeDimensions(Node* aNode, glm::vec3& aMin, glm::vec3& aMax);
		void GetSceneDimensions();
		void UpdateAnimation(std::uint32_t aIndex, float aTime);
		void PrepareNodeDescriptor(vkglTF::Node* aNode, VkDescriptorSetLayout aDescriptorSetLayout);
		void DrawNode(Node* aNode, VkCommandBuffer aCommandBuffer, std::uint32_t aRenderFlags = 0, VkPipelineLayout aPipelineLayout = VK_NULL_HANDLE, std::uint32_t aBindImageSet = 1);
		void CreateEmptyTexture(VkQueue aTransferQueue);

		Node* FindNode(Node* aParent, std::uint32_t aIndex);
		Node* NodeFromIndex(std::uint32_t aIndex);
		vkglTF::Texture* GetTexture(std::uint32_t aIndex);
		vkglTF::Texture mEmptyTexture;

	public:
		VulkanDevice* mVulkanDevice = nullptr;
		VkDescriptorPool descriptorPool{VK_NULL_HANDLE};

		struct Vertices
		{
			int count = 0;
			VkBuffer buffer{VK_NULL_HANDLE};
			VkDeviceMemory memory{VK_NULL_HANDLE};
		} vertices{};

		struct Indices
		{
			int count = 0;
			VkBuffer buffer{VK_NULL_HANDLE};
			VkDeviceMemory memory{VK_NULL_HANDLE};
		} indices{};

		std::vector<Node*> nodes;
		std::vector<Node*> linearNodes;

		std::vector<Skin*> skins;

		std::vector<Texture> textures{};
		std::vector<Material> materials{};
		std::vector<Animation> animations{};

		struct Dimensions
		{
			glm::vec3 min = glm::vec3(FLT_MAX);
			glm::vec3 max = glm::vec3(-FLT_MAX);
			glm::vec3 size;
			glm::vec3 center;
			float radius = 0.0f;
		} dimensions;

		bool metallicRoughnessWorkflow = true;
		bool buffersBound = false;
		std::string path;
	};
}
