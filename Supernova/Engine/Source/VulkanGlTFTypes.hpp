#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <limits>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

struct VulkanDevice;

namespace tinygltf
{
	struct Image;
}

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

	struct Texture
	{
		Texture();

		void UpdateDescriptor();
		void Destroy();
		void FromGlTfImage(tinygltf::Image* aGlTFimage, const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aCopyQueue);

		VulkanDevice* mVulkanDevice;
		VkDescriptorImageInfo mDescriptorImageInfo{};
		VkImage mImage;
		VkDeviceMemory mDeviceMemory;
		VkImageView mImageView;
		VkSampler mSampler;
		VkImageLayout imageLayout{};
		std::uint32_t mWidth;
		std::uint32_t mHeight;
		std::uint32_t mMipLevels;
		std::uint32_t mLayerCount;
		std::uint32_t mIndex;
	};

	struct Material
	{
		enum class AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };

		Material(VulkanDevice* device);

		void CreateDescriptorSet(VkDescriptorPool aDescriptorPool, VkDescriptorSetLayout aDescriptorSetLayout, std::uint32_t aDescriptorBindingFlags);

		VulkanDevice* mVulkanDevice;
		AlphaMode alphaMode = AlphaMode::ALPHAMODE_OPAQUE;
		float alphaCutoff = 1.0f;
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		vkglTF::Texture* baseColorTexture = nullptr;
		vkglTF::Texture* metallicRoughnessTexture = nullptr;
		vkglTF::Texture* normalTexture = nullptr;
		vkglTF::Texture* occlusionTexture = nullptr;
		vkglTF::Texture* emissiveTexture = nullptr;

		vkglTF::Texture* specularGlossinessTexture = nullptr;
		vkglTF::Texture* diffuseTexture = nullptr;

		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	};

	struct Primitive
	{
		Primitive(std::uint32_t firstIndex, std::uint32_t indexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), firstVertex{0}, vertexCount{0}, material(material) {};

		void setDimensions(glm::vec3 min, glm::vec3 max);

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
			float radius = 0.0f;
		} dimensions;
	};

	struct Mesh
	{
		Mesh(VulkanDevice* aDevice, glm::mat4 aMatrix);
		~Mesh();

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
	};

	struct Skin
	{
		std::string name;
		Node* skeletonRoot = nullptr;
		std::vector<glm::mat4> inverseBindMatrices;
		std::vector<Node*> joints;
	};

	struct Node
	{
		~Node();

		void update();

		glm::mat4 GetLocalMatrix();
		glm::mat4 GetMatrix();

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
	};

	struct AnimationChannel
	{
		enum class PathType { TRANSLATION, ROTATION, SCALE };

		PathType mPathType;
		Node* node;
		std::uint32_t samplerIndex;
	};

	struct AnimationSampler
	{
		enum class InterpolationType { LINEAR, STEP, CUBICSPLINE };

		std::vector<glm::vec4> outputsVec4;
		std::vector<float> inputs;
		InterpolationType interpolation;
	};

	struct Animation
	{
		std::vector<AnimationSampler> samplers;
		std::vector<AnimationChannel> channels;
		std::string name;
		float start = std::numeric_limits<float>::max();
		float end = std::numeric_limits<float>::min();
	};

	enum class VertexComponent { Position, Normal, UV, Color, Tangent, Joint0, Weight0 };

	struct Vertex
	{
		static VkVertexInputBindingDescription inputBindingDescription(std::uint32_t binding);
		static VkVertexInputAttributeDescription inputAttributeDescription(std::uint32_t binding, std::uint32_t location, VertexComponent component);
		static std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions(std::uint32_t binding, const std::vector<VertexComponent> components);
		static VkPipelineVertexInputStateCreateInfo* getPipelineVertexInputState(const std::vector<VertexComponent> components); // Returns the default pipeline vertex input state create info structure for the requested vertex components

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
}