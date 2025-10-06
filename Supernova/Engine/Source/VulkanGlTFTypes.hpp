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

	extern VkDescriptorSetLayout gDescriptorSetLayoutImage;
	extern VkDescriptorSetLayout gDescriptorSetLayoutUbo;
	extern VkMemoryPropertyFlags gMemoryPropertyFlags;
	extern std::uint32_t gDescriptorBindingFlags;

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

		Material(VulkanDevice* aDevice);

		void CreateDescriptorSet(VkDescriptorPool aDescriptorPool, VkDescriptorSetLayout aDescriptorSetLayout, std::uint32_t aDescriptorBindingFlags);

		VulkanDevice* mVulkanDevice;
		AlphaMode mAlphaMode;
		float mAlphaCutoff;
		float mMetallicFactor;
		float mRoughnessFactor;
		glm::vec4 mBaseColorFactor;
		vkglTF::Texture* mBaseColorTexture;
		vkglTF::Texture* mMetallicRoughnessTexture;
		vkglTF::Texture* mNormalTexture;
		vkglTF::Texture* mOcclusionTexture;
		vkglTF::Texture* mEmissiveTexture;
		vkglTF::Texture* mSpecularGlossinessTexture;
		vkglTF::Texture* mDiffuseTexture;
		VkDescriptorSet mDescriptorSet;
	};

	struct Dimensions
	{
		Dimensions() : mMin{std::numeric_limits<float>::max()}, mMax{std::numeric_limits<float>::lowest()}, mRadius{0.0f} {}

		glm::vec3 mMin;
		glm::vec3 mMax;
		glm::vec3 mSize{};
		glm::vec3 mCenter{};
		float mRadius;
	};

	struct Primitive
	{
		Primitive(std::uint32_t firstIndex, std::uint32_t indexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), firstVertex{0}, vertexCount{0}, material(material) {};

		void SetDimensions(const glm::vec3& aMin, const glm::vec3& aMax);

		Dimensions mDimensions;
		std::uint32_t firstIndex;
		std::uint32_t indexCount;
		std::uint32_t firstVertex;
		std::uint32_t vertexCount;
		Material& material;
	};

	struct Vertices
	{
		Vertices() : count{0}, buffer{VK_NULL_HANDLE}, memory{VK_NULL_HANDLE} {}

		int count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	struct Indices
	{
		Indices() : count{0}, buffer{VK_NULL_HANDLE}, memory{VK_NULL_HANDLE} {}

		int count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	struct Mesh
	{
		struct UniformBuffer
		{
			UniformBuffer() : buffer{VK_NULL_HANDLE}, memory{VK_NULL_HANDLE}, mDescriptorSet{VK_NULL_HANDLE}, mMappedData{nullptr} {}

			VkBuffer buffer;
			VkDeviceMemory memory;
			VkDescriptorBufferInfo descriptor{};
			VkDescriptorSet mDescriptorSet;
			void* mMappedData;
		};

		struct UniformBlock
		{
			UniformBlock() : mJointcount{0.0f} {}

			glm::mat4 mMatrix{};
			glm::mat4 mJointMatrix[64]{};
			float mJointcount;
		};

		Mesh(VulkanDevice* aDevice, const glm::mat4& aMatrix);
		~Mesh();

		std::vector<Primitive*> mPrimitives;
		std::string mName;
		UniformBuffer mUniformBuffer;
		UniformBlock mUniformBlock;
		VulkanDevice* mVulkanDevice;
	};

	struct Skin
	{
		Skin() : mSkeletonRoot{nullptr} {}

		std::string mName{};
		Node* mSkeletonRoot;
		std::vector<glm::mat4> inverseBindMatrices{};
		std::vector<Node*> joints{};
	};

	struct Node
	{
		Node() : mParent{nullptr}, mIndex{0}, mMesh{nullptr}, mSkin{nullptr}, mSkinIndex{-1}, mScale{1.0f} {}
		~Node();

		void update();

		glm::mat4 GetLocalMatrix() const;
		glm::mat4 GetMatrix() const;

		Node* mParent;
		std::uint32_t mIndex;
		std::vector<Node*> mChildren{};
		glm::mat4 mMatrix{};
		std::string mName{};
		Mesh* mMesh;
		Skin* mSkin;
		std::int32_t mSkinIndex;
		glm::vec3 mTranslation{};
		glm::vec3 mScale{};
		glm::quat mRotation{};
	};

	struct AnimationChannel
	{
		enum class PathType { TRANSLATION, ROTATION, SCALE };

		AnimationChannel() : mPathType{PathType::TRANSLATION}, mNode{nullptr}, mSamplerIndex{0} {}

		PathType mPathType;
		Node* mNode;
		std::uint32_t mSamplerIndex;
	};

	struct AnimationSampler
	{
		enum class InterpolationType { LINEAR, STEP, CUBICSPLINE };

		AnimationSampler() : mInterpolation{InterpolationType::LINEAR} {}

		std::vector<glm::vec4> mOutputsVec4{};
		std::vector<float> mInputs{};
		InterpolationType mInterpolation;
	};

	struct Animation
	{
		Animation() : mStart{std::numeric_limits<float>::max()}, mEnd{std::numeric_limits<float>::lowest()} {}

		std::vector<AnimationSampler> mSamplers{};
		std::vector<AnimationChannel> mChannels{};
		std::string mName{};
		float mStart;
		float mEnd;
	};

	enum class VertexComponent { Position, Normal, UV, Color, Tangent, Joint0, Weight0 };

	struct Vertex
	{
		Vertex() {}

		static VkVertexInputBindingDescription inputBindingDescription(std::uint32_t aBinding);
		static VkVertexInputAttributeDescription inputAttributeDescription(std::uint32_t aBinding, std::uint32_t aLocation, VertexComponent aComponent);
		static std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions(std::uint32_t aBinding, const std::vector<VertexComponent>& aComponents);
		static VkPipelineVertexInputStateCreateInfo* getPipelineVertexInputState(const std::vector<VertexComponent>& aComponents); // Returns the default pipeline vertex input state create info structure for the requested vertex components

		glm::vec3 mPosition{};
		glm::vec3 mNormal{};
		glm::vec2 mUV{};
		glm::vec4 mColor{};
		glm::vec4 mJoint0{};
		glm::vec4 mWeight0{};
		glm::vec4 mTangent{};
		static VkVertexInputBindingDescription mVertexInputBindingDescription;
		static std::vector<VkVertexInputAttributeDescription> mVertexInputAttributeDescriptions;
		static VkPipelineVertexInputStateCreateInfo mPipelineVertexInputStateCreateInfo;
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