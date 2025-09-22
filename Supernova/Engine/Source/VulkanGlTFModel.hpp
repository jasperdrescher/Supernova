#pragma once

#include "VulkanDevice.hpp"

#include <glm/fwd.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vulkan/vulkan_core.h>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace vkglTF
{
	enum DescriptorBindingFlags
	{
		ImageBaseColor = 0x00000001,
		ImageNormalMap = 0x00000002
	};

	extern VkDescriptorSetLayout descriptorSetLayoutImage;
	extern VkDescriptorSetLayout descriptorSetLayoutUbo;
	extern VkMemoryPropertyFlags memoryPropertyFlags;
	extern std::uint32_t descriptorBindingFlags;

	struct Node;

	// glTF texture loading class
	struct Texture
	{
		Texture() : mVulkanDevice{nullptr}, image{VK_NULL_HANDLE}, deviceMemory{VK_NULL_HANDLE}, view{VK_NULL_HANDLE}, width{0}, height{0}, mipLevels{0}, layerCount{0}, sampler{VK_NULL_HANDLE}, index{0} {}

		void updateDescriptor();
		void destroy();
		void FromGlTfImage(tinygltf::Image& aGlTFimage, std::string aPath, VulkanDevice* aDevice, VkQueue aCopyQueue);

		VulkanDevice* mVulkanDevice;
		VkImage image;
		VkImageLayout imageLayout{};
		VkDeviceMemory deviceMemory;
		VkImageView view;
		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t mipLevels;
		std::uint32_t layerCount;
		VkDescriptorImageInfo descriptor{};
		VkSampler sampler;
		std::uint32_t index;
	};

	/*
		glTF material class
	*/
	struct Material
	{
		VulkanDevice* device = nullptr;
		enum AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
		AlphaMode alphaMode = ALPHAMODE_OPAQUE;
		float alphaCutoff = 1.0f;
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		vkglTF::Texture* baseColorTexture = nullptr;
		vkglTF::Texture* metallicRoughnessTexture = nullptr;
		vkglTF::Texture* normalTexture = nullptr;
		vkglTF::Texture* occlusionTexture = nullptr;
		vkglTF::Texture* emissiveTexture = nullptr;

		vkglTF::Texture* specularGlossinessTexture;
		vkglTF::Texture* diffuseTexture;

		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

		Material(VulkanDevice* device) : device(device) {};
		void CreateDescriptorSet(VkDescriptorPool aDescriptorPool, VkDescriptorSetLayout aDescriptorSetLayout, std::uint32_t aDescriptorBindingFlags);
	};

	/*
		glTF primitive
	*/
	struct Primitive
	{
		std::uint32_t firstIndex;
		std::uint32_t indexCount;
		std::uint32_t firstVertex;
		std::uint32_t vertexCount;
		Material& material;

		struct Dimensions
		{
			glm::vec3 min = glm::vec3(FLT_MAX);
			glm::vec3 max = glm::vec3(-FLT_MAX);
			glm::vec3 size;
			glm::vec3 center;
			float radius;
		} dimensions;

		void setDimensions(glm::vec3 min, glm::vec3 max);
		Primitive(std::uint32_t firstIndex, std::uint32_t indexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), material(material) {};
	};

	/*
		glTF mesh
	*/
	struct Mesh
	{
		VulkanDevice* mVulkanDevice;

		std::vector<Primitive*> primitives;
		std::string name;

		struct UniformBuffer
		{
			VkBuffer buffer;
			VkDeviceMemory memory;
			VkDescriptorBufferInfo descriptor;
			VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
			void* mapped;
		} uniformBuffer;

		struct UniformBlock
		{
			glm::mat4 matrix;
			glm::mat4 jointMatrix[64]{};
			float jointcount{0};
		} uniformBlock;

		Mesh(VulkanDevice* aDevice, glm::mat4 aMatrix);
		~Mesh();
	};

	/*
		glTF skin
	*/
	struct Skin
	{
		std::string name;
		Node* skeletonRoot = nullptr;
		std::vector<glm::mat4> inverseBindMatrices;
		std::vector<Node*> joints;
	};

	/*
		glTF node
	*/
	struct Node
	{
		Node* parent;
		std::uint32_t index;
		std::vector<Node*> children;
		glm::mat4 matrix;
		std::string name;
		Mesh* mesh;
		Skin* skin;
		int32_t skinIndex = -1;
		glm::vec3 translation{};
		glm::vec3 scale{1.0f};
		glm::quat rotation{};
		glm::mat4 localMatrix();
		glm::mat4 getMatrix();
		void update();
		~Node();
	};

	/*
		glTF animation channel
	*/
	struct AnimationChannel
	{
		enum PathType { TRANSLATION, ROTATION, SCALE };
		PathType path;
		Node* node;
		std::uint32_t samplerIndex;
	};

	/*
		glTF animation sampler
	*/
	struct AnimationSampler
	{
		enum InterpolationType { LINEAR, STEP, CUBICSPLINE };
		InterpolationType interpolation;
		std::vector<float> inputs;
		std::vector<glm::vec4> outputsVec4;
	};

	/*
		glTF animation
	*/
	struct Animation
	{
		std::string name;
		std::vector<AnimationSampler> samplers;
		std::vector<AnimationChannel> channels;
		float start = std::numeric_limits<float>::max();
		float end = std::numeric_limits<float>::min();
	};

	/*
		glTF default vertex layout with easy Vulkan mapping functions
	*/
	enum class VertexComponent { Position, Normal, UV, Color, Tangent, Joint0, Weight0 };

	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec4 color;
		glm::vec4 joint0;
		glm::vec4 weight0;
		glm::vec4 tangent;
		static VkVertexInputBindingDescription vertexInputBindingDescription;
		static std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
		static VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
		static VkVertexInputBindingDescription inputBindingDescription(std::uint32_t binding);
		static VkVertexInputAttributeDescription inputAttributeDescription(std::uint32_t binding, std::uint32_t location, VertexComponent component);
		static std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions(std::uint32_t binding, const std::vector<VertexComponent> components);
		/** @brief Returns the default pipeline vertex input state create info structure for the requested vertex components */
		static VkPipelineVertexInputStateCreateInfo* getPipelineVertexInputState(const std::vector<VertexComponent> components);
	};

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

	/*
		glTF model loading and rendering class
	*/
	class Model
	{
	private:
		vkglTF::Texture* getTexture(std::uint32_t index);
		vkglTF::Texture emptyTexture;
		void createEmptyTexture(VkQueue transferQueue);
	public:
		VulkanDevice* device;
		VkDescriptorPool descriptorPool;

		struct Vertices
		{
			int count;
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertices;
		struct Indices
		{
			int count;
			VkBuffer buffer;
			VkDeviceMemory memory;
		} indices;

		std::vector<Node*> nodes;
		std::vector<Node*> linearNodes;

		std::vector<Skin*> skins;

		std::vector<Texture> textures;
		std::vector<Material> materials;
		std::vector<Animation> animations;

		struct Dimensions
		{
			glm::vec3 min = glm::vec3(FLT_MAX);
			glm::vec3 max = glm::vec3(-FLT_MAX);
			glm::vec3 size;
			glm::vec3 center;
			float radius;
		} dimensions;

		bool metallicRoughnessWorkflow = true;
		bool buffersBound = false;
		std::string path;

		Model() {};
		~Model();
		void loadNode(vkglTF::Node* parent, const tinygltf::Node& node, std::uint32_t nodeIndex, const tinygltf::Model& model, std::vector<std::uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalscale);
		void loadSkins(tinygltf::Model& gltfModel);
		void LoadImages(tinygltf::Model& aGlTFModel, VulkanDevice* aDevice, VkQueue aTransferQueue);
		void loadMaterials(tinygltf::Model& gltfModel);
		void loadAnimations(tinygltf::Model& gltfModel);
		void loadFromFile(const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aTransferQueue, std::uint32_t aFileLoadingFlags = vkglTF::FileLoadingFlags::None, float aScale = 1.0f);
		void bindBuffers(VkCommandBuffer commandBuffer);
		void drawNode(Node* node, VkCommandBuffer commandBuffer, std::uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, std::uint32_t bindImageSet = 1);
		void draw(VkCommandBuffer commandBuffer, std::uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, std::uint32_t bindImageSet = 1);
		void getNodeDimensions(Node* node, glm::vec3& min, glm::vec3& max);
		void getSceneDimensions();
		void updateAnimation(std::uint32_t index, float time);
		Node* findNode(Node* parent, std::uint32_t index);
		Node* nodeFromIndex(std::uint32_t index);
		void prepareNodeDescriptor(vkglTF::Node* node, VkDescriptorSetLayout descriptorSetLayout);
	};
}
