#include "VulkanGlTFTypes.hpp"

#include "Core/Types.hpp"
#include "Math/Functions.hpp"
#include "Math/Types.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTools.hpp"

#include <cstddef>
#include <cstring>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkglTF
{
	Model::Model()
		: buffersBound{false}
	{
	}

	Texture::Texture()
		: mVulkanDevice{nullptr}
		, mImage{VK_NULL_HANDLE}
		, mDeviceMemory{VK_NULL_HANDLE}
		, mImageView{VK_NULL_HANDLE}
		, mWidth{0}
		, mHeight{0}
		, mMipLevels{0}
		, mLayerCount{0}
		, mSampler{VK_NULL_HANDLE}
		, mIndex{0}
	{
	}

	Material::Material(VulkanDevice* aDevice)
		: mVulkanDevice{aDevice}
		, mAlphaMode{AlphaMode::Opaque}
		, mAlphaCutoff{1.0f}
		, mMetallicFactor{1.0f}
		, mRoughnessFactor{1.0f}
		, mBaseColorFactor{1.0f}
		, mBaseColorTexture{nullptr}
		, mMetallicRoughnessTexture{nullptr}
		, mNormalTexture{nullptr}
		, mOcclusionTexture{nullptr}
		, mEmissiveTexture{nullptr}
		, mSpecularGlossinessTexture{nullptr}
		, mDiffuseTexture{nullptr}
		, mDescriptorSet{VK_NULL_HANDLE}
	{
	}

	void Texture::UpdateDescriptor()
	{
		mDescriptorImageInfo.sampler = mSampler;
		mDescriptorImageInfo.imageView = mImageView;
		mDescriptorImageInfo.imageLayout = imageLayout;
	}

	void Texture::Destroy()
	{
		if (mVulkanDevice)
		{
			vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mImageView, nullptr);
			vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mImage, nullptr);
			vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mDeviceMemory, nullptr);
			vkDestroySampler(mVulkanDevice->mLogicalVkDevice, mSampler, nullptr);
		}
	}

	void vkglTF::Primitive::SetDimensions(const Math::Vector3f& aMin, const Math::Vector3f& aMax)
	{
		mDimensions.mMin = aMin;
		mDimensions.mMax = aMax;
		mDimensions.mSize = aMax - aMin;
		mDimensions.mCenter = (aMin + aMax) / 2.0f;
		mDimensions.mRadius = Math::Distance(aMin, aMax) / 2.0f;
	}

	vkglTF::Mesh::Mesh(VulkanDevice* aDevice, const Math::Matrix4f& aMatrix)
	{
		mVulkanDevice = aDevice;
		mUniformBlock.mMatrix = aMatrix;
		VK_CHECK_RESULT(aDevice->CreateBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			sizeof(mUniformBlock),
			&mUniformBuffer.buffer,
			&mUniformBuffer.memory,
			&mUniformBlock));
		VK_CHECK_RESULT(vkMapMemory(aDevice->mLogicalVkDevice, mUniformBuffer.memory, 0, sizeof(mUniformBlock), 0, &mUniformBuffer.mMappedData));
		mUniformBuffer.descriptor = {mUniformBuffer.buffer, 0, sizeof(mUniformBlock)};
	};

	Mesh::~Mesh()
	{
		vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, mUniformBuffer.buffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mUniformBuffer.memory, nullptr);
		for (vkglTF::Primitive* primitive : mPrimitives)
		{
			delete primitive;
		}
	}

	Math::Matrix4f Node::GetLocalMatrix() const
	{
		return Math::Translate(Math::Matrix4f(1.0f), mTranslation) * Math::Matrix4f(mRotation) * Math::Scale(Math::Matrix4f(1.0f), mScale) * mMatrix;
	}

	Math::Matrix4f Node::GetMatrix() const
	{
		Math::Matrix4f m = GetLocalMatrix();
		vkglTF::Node* p = mParent;
		while (p)
		{
			m = p->GetLocalMatrix() * m;
			p = p->mParent;
		}
		return m;
	}

	void Node::update()
	{
		if (mMesh)
		{
			Math::Matrix4f m = GetMatrix();
			if (mSkin)
			{
				mMesh->mUniformBlock.mMatrix = m;
				// Update join matrices
				Math::Matrix4f inverseTransform = Math::Inverse(m);
				for (Core::size i = 0; i < mSkin->joints.size(); i++)
				{
					vkglTF::Node* jointNode = mSkin->joints[i];
					Math::Matrix4f jointMat = jointNode->GetMatrix() * mSkin->inverseBindMatrices[i];
					jointMat = inverseTransform * jointMat;
					mMesh->mUniformBlock.mJointMatrix[i] = jointMat;
				}
				mMesh->mUniformBlock.mJointcount = static_cast<float>(mSkin->joints.size());
				std::memcpy(mMesh->mUniformBuffer.mMappedData, &mMesh->mUniformBlock, sizeof(mMesh->mUniformBlock));
			}
			else
			{
				std::memcpy(mMesh->mUniformBuffer.mMappedData, &m, sizeof(Math::Matrix4f));
			}
		}

		for (vkglTF::Node* child : mChildren)
		{
			child->update();
		}
	}

	Node::~Node()
	{
		if (mMesh)
		{
			delete mMesh;
		}

		for (vkglTF::Node* child : mChildren)
		{
			delete child;
		}
	}

	VkVertexInputBindingDescription Vertex::mVertexInputBindingDescription;
	std::vector<VkVertexInputAttributeDescription> Vertex::mVertexInputAttributeDescriptions;
	VkPipelineVertexInputStateCreateInfo Vertex::mPipelineVertexInputStateCreateInfo;

	VkVertexInputBindingDescription vkglTF::Vertex::inputBindingDescription(Core::uint32 aBinding)
	{
		return VkVertexInputBindingDescription({aBinding, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX});
	}

	VkVertexInputAttributeDescription vkglTF::Vertex::inputAttributeDescription(Core::uint32 aBinding, Core::uint32 aLocation, VertexComponent aComponent)
	{
		switch (aComponent)
		{
			case VertexComponent::Position:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, mPosition)});
			case VertexComponent::Normal:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, mNormal)});
			case VertexComponent::UV:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, mUV)});
			case VertexComponent::Color:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, mColor)});
			case VertexComponent::Tangent:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, mTangent)});
			case VertexComponent::Joint0:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, mJoint0)});
			case VertexComponent::Weight0:
				return VkVertexInputAttributeDescription({aLocation, aBinding, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, mWeight0)});
			default:
				return VkVertexInputAttributeDescription({});
		}
	}

	std::vector<VkVertexInputAttributeDescription> vkglTF::Vertex::inputAttributeDescriptions(Core::uint32 aBinding, const std::vector<VertexComponent>& aComponents)
	{
		std::vector<VkVertexInputAttributeDescription> result;
		Core::uint32 location = 0;
		for (VertexComponent component : aComponents)
		{
			result.push_back(Vertex::inputAttributeDescription(aBinding, location, component));
			location++;
		}
		return result;
	}

	/** @brief Returns the default pipeline vertex input state create info structure for the requested vertex components */
	VkPipelineVertexInputStateCreateInfo* vkglTF::Vertex::getPipelineVertexInputState(const std::vector<VertexComponent>& aComponents)
	{
		mVertexInputBindingDescription = Vertex::inputBindingDescription(0);
		Vertex::mVertexInputAttributeDescriptions = Vertex::inputAttributeDescriptions(0, aComponents);
		mPipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		mPipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
		mPipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &Vertex::mVertexInputBindingDescription;
		mPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<Core::uint32>(Vertex::mVertexInputAttributeDescriptions.size());
		mPipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = Vertex::mVertexInputAttributeDescriptions.data();
		return &mPipelineVertexInputStateCreateInfo;
	}
};
