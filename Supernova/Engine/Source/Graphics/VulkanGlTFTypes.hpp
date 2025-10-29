#pragma once

#include "Core/Types.hpp"
#include "Math/Types.hpp"

#include <filesystem>
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
	extern Core::uint32 gDescriptorBindingFlags;

	struct Node;

	struct Texture
	{
		Texture();

		void CreateTexture(tinygltf::Image* aGlTFimage, const std::filesystem::path& aModelPath, VulkanDevice* aDevice, VkQueue aCopyQueue);
		void UpdateDescriptor();
		void Destroy();

		VulkanDevice* mVulkanDevice;
		VkDescriptorImageInfo mDescriptorImageInfo{};
		VkImage mImage;
		VkDeviceMemory mDeviceMemory;
		VkImageView mImageView;
		VkSampler mSampler;
		VkImageLayout imageLayout{};
		Core::uint32 mWidth;
		Core::uint32 mHeight;
		Core::uint32 mMipLevels;
		Core::uint32 mLayerCount;
		Core::uint32 mIndex;

	private:
		void CreateFromKtxTexture(tinygltf::Image* aGlTFimage, const std::filesystem::path& aModelPath, VkFormat& aFormat, VulkanDevice* aDevice, VkQueue aCopyQueue);
		void CreateFromIncludedTexture(tinygltf::Image* aGlTFimage, VkFormat& aFormat, VulkanDevice* aDevice, VkQueue aCopyQueue);
	};

	struct Material
	{
		enum class AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };

		Material(VulkanDevice* aDevice);

		void CreateDescriptorSet(VkDescriptorPool aDescriptorPool, VkDescriptorSetLayout aDescriptorSetLayout, Core::uint32 aDescriptorBindingFlags);

		VulkanDevice* mVulkanDevice;
		AlphaMode mAlphaMode;
		float mAlphaCutoff;
		float mMetallicFactor;
		float mRoughnessFactor;
		Math::Vector4f mBaseColorFactor;
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

		Math::Vector3f mMin;
		Math::Vector3f mMax;
		Math::Vector3f mSize{};
		Math::Vector3f mCenter{};
		float mRadius;
	};

	struct Primitive
	{
		Primitive(Core::uint32 firstIndex, Core::uint32 indexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), firstVertex{0}, vertexCount{0}, material(material) {};

		void SetDimensions(const Math::Vector3f& aMin, const Math::Vector3f& aMax);

		Dimensions mDimensions;
		Core::uint32 firstIndex;
		Core::uint32 indexCount;
		Core::uint32 firstVertex;
		Core::uint32 vertexCount;
		Material& material;
	};

	struct Vertices
	{
		Vertices() : mCount{0}, mBuffer{VK_NULL_HANDLE}, mMemory{VK_NULL_HANDLE} {}

		int mCount;
		VkBuffer mBuffer;
		VkDeviceMemory mMemory;
	};

	struct Indices
	{
		Indices() : mCount{0}, mBuffer{VK_NULL_HANDLE}, mMemory{VK_NULL_HANDLE} {}

		int mCount;
		VkBuffer mBuffer;
		VkDeviceMemory mMemory;
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

			Math::Matrix4f mMatrix{};
			Math::Matrix4f mJointMatrix[64]{};
			float mJointcount;
		};

		Mesh(VulkanDevice* aDevice, const Math::Matrix4f& aMatrix);
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
		std::vector<Math::Matrix4f> inverseBindMatrices{};
		std::vector<Node*> joints{};
	};

	struct Node
	{
		Node() : mParent{nullptr}, mIndex{0}, mMesh{nullptr}, mSkin{nullptr}, mSkinIndex{-1}, mScale{1.0f} {}
		~Node();

		void update();

		Math::Matrix4f GetLocalMatrix() const;
		Math::Matrix4f GetMatrix() const;

		Node* mParent;
		Core::uint32 mIndex;
		std::vector<Node*> mChildren{};
		Math::Matrix4f mMatrix{};
		std::string mName{};
		Mesh* mMesh;
		Skin* mSkin;
		Core::int32 mSkinIndex;
		Math::Vector3f mTranslation{};
		Math::Vector3f mScale{};
		Math::Quaternionf mRotation{};
	};

	struct AnimationChannel
	{
		enum class PathType { TRANSLATION, ROTATION, SCALE };

		AnimationChannel() : mPathType{PathType::TRANSLATION}, mNode{nullptr}, mSamplerIndex{0} {}

		PathType mPathType;
		Node* mNode;
		Core::uint32 mSamplerIndex;
	};

	struct AnimationSampler
	{
		enum class InterpolationType { LINEAR, STEP, CUBICSPLINE };

		AnimationSampler() : mInterpolation{InterpolationType::LINEAR} {}

		std::vector<Math::Vector4f> mOutputsVec4{};
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

		static VkVertexInputBindingDescription inputBindingDescription(Core::uint32 aBinding);
		static VkVertexInputAttributeDescription inputAttributeDescription(Core::uint32 aBinding, Core::uint32 aLocation, VertexComponent aComponent);
		static std::vector<VkVertexInputAttributeDescription> inputAttributeDescriptions(Core::uint32 aBinding, const std::vector<VertexComponent>& aComponents);
		static VkPipelineVertexInputStateCreateInfo* getPipelineVertexInputState(const std::vector<VertexComponent>& aComponents); // Returns the default pipeline vertex input state create info structure for the requested vertex components

		Math::Vector3f mPosition{};
		Math::Vector3f mNormal{};
		Math::Vector2f mUV{};
		Math::Vector4f mColor{};
		Math::Vector4f mJoint0{};
		Math::Vector4f mWeight0{};
		Math::Vector4f mTangent{};
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