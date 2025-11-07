#include "ModelManager.hpp"

#include "Core/Types.hpp"
#include "Math/Functions.hpp"
#include "Math/Types.hpp"
#include "ModelFlags.hpp"
#include "TextureManager.hpp"
#include "Timer.hpp"
#include "UniqueIdentifier.hpp"
#include "VulkanDevice.hpp"
#include "VulkanTools.hpp"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cassert>
#include <cstdint>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vulkan/vulkan_core.h>
#include <filesystem>
#include <vector>
#include <cstring>
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

VkDescriptorSetLayout vkglTF::gDescriptorSetLayoutImage = VK_NULL_HANDLE;
VkDescriptorSetLayout vkglTF::gDescriptorSetLayoutUbo = VK_NULL_HANDLE;
VkMemoryPropertyFlags vkglTF::gMemoryPropertyFlags = 0;
std::uint32_t vkglTF::gDescriptorBindingFlags = vkglTF::DescriptorBindingFlags::ImageBaseColor;

namespace VulkanGlTFModelLocal
{
	// We use a custom image loading function with tinyglTF, so we can do custom stuff loading ktx textures
	static bool loadImageDataFunc(tinygltf::Image* aImage, const int aImageIndex, std::string* aError, std::string* aWarning, int aReqWidth, int aReqHeight, const unsigned char* aBytes, int aSize, void* aUserData)
	{
		// KTX files will be handled by our own code
		if (aImage->uri.find_last_of(".") != std::string::npos)
		{
			if (aImage->uri.substr(aImage->uri.find_last_of(".") + 1) == "ktx")
			{
				return true;
			}
		}

		return tinygltf::LoadImageData(aImage, aImageIndex, aError, aWarning, aReqWidth, aReqHeight, aBytes, aSize, aUserData);
	}

	static bool LoadImageDataFuncEmpty(tinygltf::Image* /*aImage*/, const int /*aImageIndex*/, std::string* /*aError*/, std::string* /*aWarning*/, int /*aReqWidth*/, int /*aReqHeight*/, const unsigned char* /*aBytes*/, int /*aSize*/, void* /*aUserData*/)
	{
		// This function will be used for samples that don't require images to be loaded
		return true;
	}
}

ModelManager::ModelManager(const std::shared_ptr<TextureManager>& aTextureManager)
	: mTextureManager{aTextureManager}
	, mVulkanDevice{nullptr}
	, descriptorPool{VK_NULL_HANDLE}
{
}

ModelManager::~ModelManager()
{
	for (const std::pair<UniqueIdentifier, vkglTF::Model*>& pair : mModels)
	{
		vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, pair.second->vertices.mBuffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalVkDevice, pair.second->vertices.mMemory, nullptr);
		vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, pair.second->indices.mBuffer, nullptr);
		vkFreeMemory(mVulkanDevice->mLogicalVkDevice, pair.second->indices.mMemory, nullptr);

		for (vkglTF::Node*& node : pair.second->nodes)
		{
			delete node;
		}

		for (vkglTF::Skin*& skin : pair.second->skins)
		{
			delete skin;
		}

		for (vkglTF::Texture& texture : pair.second->textures)
		{
			texture.Destroy();
		}

		pair.second->mEmptyTexture.Destroy();
	}

	if (vkglTF::gDescriptorSetLayoutUbo != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, vkglTF::gDescriptorSetLayoutUbo, nullptr);
		vkglTF::gDescriptorSetLayoutUbo = VK_NULL_HANDLE;
	}

	if (vkglTF::gDescriptorSetLayoutImage != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, vkglTF::gDescriptorSetLayoutImage, nullptr);
		vkglTF::gDescriptorSetLayoutImage = VK_NULL_HANDLE;
	}

	vkDestroyDescriptorPool(mVulkanDevice->mLogicalVkDevice, descriptorPool, nullptr);
}

void ModelManager::LoadNode(vkglTF::Model& aModel, tinygltf::Model* aGltfModel, vkglTF::Node* aParent, const tinygltf::Node* aNode, std::uint32_t aNodeIndex, std::vector<std::uint32_t>& aIndexBuffer, std::vector<vkglTF::Vertex>& aVertexBuffer, float aGlobalscale)
{
	vkglTF::Node* newNode = new vkglTF::Node{};
	newNode->mIndex = aNodeIndex;
	newNode->mParent = aParent;
	newNode->mName = aNode->name;
	newNode->mSkinIndex = aNode->skin;
	newNode->mMatrix = Math::Matrix4f{1.0f};

	// Generate local node matrix
	Math::Vector3f translation = Math::Vector3f{0.0f};
	if (aNode->translation.size() == 3)
	{
		translation = Math::MakeVector3f(aNode->translation.data());
		newNode->mTranslation = translation;
	}

	if (aNode->rotation.size() == 4)
	{
		const Math::Quaternionf q = Math::MakeQuaternion(aNode->rotation.data());
		newNode->mRotation = Math::Matrix4f(q);
	}

	Math::Vector3f scale = Math::Vector3f{1.0f};
	if (aNode->scale.size() == 3)
	{
		scale = Math::MakeVector3f(aNode->scale.data());
		newNode->mScale = scale;
	}

	if (aNode->matrix.size() == 16)
	{
		newNode->mMatrix = Math::MakeMatrix(aNode->matrix.data());
		if (aGlobalscale != 1.0f)
		{
			//newNode->matrix = Math::Scale(newNode->matrix, Math::Vector3f(globalscale));
		}
	};

	// Node with children
	if (aNode->children.size() > 0)
	{
		for (size_t i = 0; i < aNode->children.size(); i++)
		{
			LoadNode(aModel, aGltfModel, newNode, &aGltfModel->nodes[aNode->children[i]], aNode->children[i], aIndexBuffer, aVertexBuffer, aGlobalscale);
		}
	}

	// Node contains mesh data
	if (aNode->mesh > -1)
	{
		const tinygltf::Mesh& mesh = aGltfModel->meshes[aNode->mesh];
		vkglTF::Mesh* newMesh = new vkglTF::Mesh(mVulkanDevice, newNode->mMatrix);
		newMesh->mName = mesh.name;
		for (size_t j = 0; j < mesh.primitives.size(); j++)
		{
			const tinygltf::Primitive& primitive = mesh.primitives[j];
			if (primitive.indices < 0)
			{
				continue;
			}
			std::uint32_t indexStart = static_cast<std::uint32_t>(aIndexBuffer.size());
			std::uint32_t vertexStart = static_cast<std::uint32_t>(aVertexBuffer.size());
			std::uint32_t indexCount = 0;
			std::uint32_t vertexCount = 0;
			Math::Vector3f posMin{};
			Math::Vector3f posMax{};
			bool hasSkin = false;
			// Vertices
			{
				const float* bufferPos = nullptr;
				const float* bufferNormals = nullptr;
				const float* bufferTexCoords = nullptr;
				const float* bufferColors = nullptr;
				const float* bufferTangents = nullptr;
				std::uint32_t numColorComponents = 0;
				const uint16_t* bufferJoints = nullptr;
				const float* bufferWeights = nullptr;

				// Position attribute is required
				assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

				const tinygltf::Accessor& posAccessor = aGltfModel->accessors[primitive.attributes.find("POSITION")->second];
				const tinygltf::BufferView& posView = aGltfModel->bufferViews[posAccessor.bufferView];
				bufferPos = reinterpret_cast<const float*>(&(aGltfModel->buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
				posMin = Math::Vector3f(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
				posMax = Math::Vector3f(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);

				if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
				{
					const tinygltf::Accessor& normAccessor = aGltfModel->accessors[primitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& normView = aGltfModel->bufferViews[normAccessor.bufferView];
					bufferNormals = reinterpret_cast<const float*>(&(aGltfModel->buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
				}

				if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
				{
					const tinygltf::Accessor& uvAccessor = aGltfModel->accessors[primitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& uvView = aGltfModel->bufferViews[uvAccessor.bufferView];
					bufferTexCoords = reinterpret_cast<const float*>(&(aGltfModel->buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
				}

				if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
				{
					const tinygltf::Accessor& colorAccessor = aGltfModel->accessors[primitive.attributes.find("COLOR_0")->second];
					const tinygltf::BufferView& colorView = aGltfModel->bufferViews[colorAccessor.bufferView];
					// Color buffer are either of type vec3 or vec4
					numColorComponents = colorAccessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
					bufferColors = reinterpret_cast<const float*>(&(aGltfModel->buffers[colorView.buffer].data[colorAccessor.byteOffset + colorView.byteOffset]));
				}

				if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
				{
					const tinygltf::Accessor& tangentAccessor = aGltfModel->accessors[primitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView& tangentView = aGltfModel->bufferViews[tangentAccessor.bufferView];
					bufferTangents = reinterpret_cast<const float*>(&(aGltfModel->buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]));
				}

				// Skinning
				// Joints
				if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
				{
					const tinygltf::Accessor& jointAccessor = aGltfModel->accessors[primitive.attributes.find("JOINTS_0")->second];
					const tinygltf::BufferView& jointView = aGltfModel->bufferViews[jointAccessor.bufferView];
					bufferJoints = reinterpret_cast<const uint16_t*>(&(aGltfModel->buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]));
				}

				if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
				{
					const tinygltf::Accessor& uvAccessor = aGltfModel->accessors[primitive.attributes.find("WEIGHTS_0")->second];
					const tinygltf::BufferView& uvView = aGltfModel->bufferViews[uvAccessor.bufferView];
					bufferWeights = reinterpret_cast<const float*>(&(aGltfModel->buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
				}

				hasSkin = (bufferJoints && bufferWeights);

				vertexCount = static_cast<std::uint32_t>(posAccessor.count);

				for (size_t v = 0; v < posAccessor.count; v++)
				{
					vkglTF::Vertex vert{};
					vert.mPosition = Math::Vector4f(Math::MakeVector3f(&bufferPos[v * 3]), 1.0f);
					vert.mNormal = Math::Normalize(Math::Vector3f(bufferNormals ? Math::MakeVector3f(&bufferNormals[v * 3]) : Math::Vector3f(0.0f)));
					vert.mUV = bufferTexCoords ? Math::MakeVector2f(&bufferTexCoords[v * 2]) : Math::Vector3f(0.0f);
					if (bufferColors)
					{
						switch (numColorComponents)
						{
							case 3:
								vert.mColor = Math::Vector4f(Math::MakeVector3f(&bufferColors[v * 3]), 1.0f);
								break;
							case 4:
								vert.mColor = Math::MakeVector4f(&bufferColors[v * 4]);
								break;
						}
					}
					else
					{
						vert.mColor = Math::Vector4f(1.0f);
					}
					vert.mTangent = bufferTangents ? Math::Vector4f(Math::MakeVector4f(&bufferTangents[v * 4])) : Math::Vector4f(0.0f);
					vert.mJoint0 = hasSkin ? Math::Vector4f(Math::MakeVector4f(&bufferJoints[v * 4])) : Math::Vector4f(0.0f);
					vert.mWeight0 = hasSkin ? Math::MakeVector4f(&bufferWeights[v * 4]) : Math::Vector4f(0.0f);
					aVertexBuffer.push_back(vert);
				}
			}
			// Indices
			{
				const tinygltf::Accessor& accessor = aGltfModel->accessors[primitive.indices];
				const tinygltf::BufferView& bufferView = aGltfModel->bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = aGltfModel->buffers[bufferView.buffer];

				indexCount = static_cast<std::uint32_t>(accessor.count);

				switch (accessor.componentType)
				{
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
					{
						std::uint32_t* buf = new std::uint32_t[accessor.count];
						std::memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(std::uint32_t));
						for (size_t index = 0; index < accessor.count; index++)
						{
							aIndexBuffer.push_back(buf[index] + vertexStart);
						}

						delete[] buf;

						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
					{
						uint16_t* buf = new uint16_t[accessor.count];
						std::memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
						for (size_t index = 0; index < accessor.count; index++)
						{
							aIndexBuffer.push_back(buf[index] + vertexStart);
						}

						delete[] buf;

						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
					{
						uint8_t* buf = new uint8_t[accessor.count];
						std::memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
						for (size_t index = 0; index < accessor.count; index++)
						{
							aIndexBuffer.push_back(buf[index] + vertexStart);
						}

						delete[] buf;

						break;
					}
					default:
						std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
						return;
				}
			}

			vkglTF::Primitive* newPrimitive = new vkglTF::Primitive(indexStart, indexCount, primitive.material > -1 ? aModel.materials[primitive.material] : aModel.materials.back());
			newPrimitive->firstVertex = vertexStart;
			newPrimitive->vertexCount = vertexCount;
			newPrimitive->SetDimensions(posMin, posMax);
			newMesh->mPrimitives.push_back(newPrimitive);
		}

		newNode->mMesh = newMesh;
	}

	if (aParent)
	{
		aParent->mChildren.push_back(newNode);
	}
	else
	{
		aModel.nodes.push_back(newNode);
	}

	aModel.linearNodes.push_back(newNode);
}

void ModelManager::LoadSkins(vkglTF::Model& aModel, tinygltf::Model* aGltfModel)
{
	for (const tinygltf::Skin& source : aGltfModel->skins)
	{
		vkglTF::Skin* newSkin = new vkglTF::Skin{};
		newSkin->mName = source.name;

		// Find skeleton root node
		if (source.skeleton > -1)
		{
			newSkin->mSkeletonRoot = NodeFromIndex(aModel, source.skeleton);
		}

		// Find joint nodes
		for (int jointIndex : source.joints)
		{
			vkglTF::Node* node = NodeFromIndex(aModel, jointIndex);
			if (node)
			{
				newSkin->joints.push_back(NodeFromIndex(aModel, jointIndex));
			}
		}

		// Get inverse bind matrices from buffer
		if (source.inverseBindMatrices > -1)
		{
			const tinygltf::Accessor& accessor = aGltfModel->accessors[source.inverseBindMatrices];
			const tinygltf::BufferView& bufferView = aGltfModel->bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = aGltfModel->buffers[bufferView.buffer];
			newSkin->inverseBindMatrices.resize(accessor.count);
			std::memcpy(newSkin->inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(Math::Matrix4f));
		}

		aModel.skins.push_back(newSkin);
	}
}

void ModelManager::LoadImages(vkglTF::Model& aModel, tinygltf::Model* aGltfModel)
{
	for (const tinygltf::Image& gltfImage : aGltfModel->images)
	{
		vkglTF::Image image;
		image.component = gltfImage.component;
		image.width = gltfImage.width;
		image.height = gltfImage.height;
		image.uri = gltfImage.uri;
		image.name = gltfImage.name;
		image.image = gltfImage.image;

		if (gltfImage.extensions.find("KHR_texture_basisu") != gltfImage.extensions.end())
		{
			if (gltfImage.extensions.at("KHR_texture_basisu").Has("arrayLayers"))
			{
				image.layers = gltfImage.extensions.at("KHR_texture_basisu").Get("arrayLayers").GetNumberAsInt();
			}
		}

		vkglTF::Texture texture = mTextureManager.lock()->CreateTexture(aModel.path.relative_path(), image);
		texture.mIndex = static_cast<std::uint32_t>(aModel.textures.size());
		aModel.textures.push_back(texture);
	}

	// Create an empty texture to be used for empty material images
	aModel.mEmptyTexture = mTextureManager.lock()->CreateEmptyTexture();
}

vkglTF::Texture* ModelManager::GetTexture(vkglTF::Model& aModel, std::uint32_t aIndex)
{
	if (aIndex < aModel.textures.size())
		return &aModel.textures[aIndex];

	return nullptr;
}

void ModelManager::LoadMaterials(vkglTF::Model& aModel, tinygltf::Model* aGltfModel)
{
	for (const tinygltf::Material& gltfMaterial : aGltfModel->materials)
	{
		vkglTF::Material material(mVulkanDevice);

		if (gltfMaterial.pbrMetallicRoughness.baseColorTexture.index != -1)
		{
			material.mBaseColorTexture = GetTexture(aModel, aGltfModel->textures[gltfMaterial.pbrMetallicRoughness.baseColorTexture.index].source);
		}

		if (gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
		{
			material.mMetallicRoughnessTexture = GetTexture(aModel, aGltfModel->textures[gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index].source);
		}

		material.mBaseColorFactor = Math::MakeVector4f(gltfMaterial.pbrMetallicRoughness.baseColorFactor.data());
		material.mRoughnessFactor = static_cast<float>(gltfMaterial.pbrMetallicRoughness.roughnessFactor);
		material.mMetallicFactor = static_cast<float>(gltfMaterial.pbrMetallicRoughness.metallicFactor);

		if (gltfMaterial.normalTexture.index != -1)
		{
			material.mNormalTexture = GetTexture(aModel, aGltfModel->textures[gltfMaterial.normalTexture.index].source);
		}
		else
		{
			material.mNormalTexture = &aModel.mEmptyTexture;
		}

		if (gltfMaterial.emissiveTexture.index != -1)
		{
			material.mEmissiveTexture = GetTexture(aModel, aGltfModel->textures[gltfMaterial.emissiveTexture.index].source);
		}

		if (gltfMaterial.occlusionTexture.index != -1)
		{
			material.mOcclusionTexture = GetTexture(aModel, aGltfModel->textures[gltfMaterial.occlusionTexture.index].source);
		}

		const std::string alphaMode = gltfMaterial.alphaMode;
		if (alphaMode == "OPAQUE")
		{
			material.mAlphaMode = vkglTF::Material::AlphaMode::Opaque;
		}
		else if (alphaMode == "BLEND")
		{
			material.mAlphaMode = vkglTF::Material::AlphaMode::Blend;
		}
		else if (alphaMode == "MASK")
		{
			material.mAlphaMode = vkglTF::Material::AlphaMode::Mask;
		}

		material.mAlphaCutoff = static_cast<float>(gltfMaterial.alphaCutoff);

		aModel.materials.push_back(material);
	}

	// Push a default material at the end of the list for meshes with no material assigned
	aModel.materials.push_back(vkglTF::Material(mVulkanDevice));
}

void ModelManager::LoadAnimations(vkglTF::Model& aModel, tinygltf::Model* aGltfModel)
{
	for (const tinygltf::Animation& gltfAnimation : aGltfModel->animations)
	{
		vkglTF::Animation animation{};
		animation.mName = gltfAnimation.name;

		if (gltfAnimation.name.empty())
		{
			animation.mName = std::to_string(aModel.animations.size());
		}

		for (const tinygltf::AnimationSampler& gltfAnimationSampler : gltfAnimation.samplers)
		{
			vkglTF::AnimationSampler sampler{};

			if (gltfAnimationSampler.interpolation == "LINEAR")
			{
				sampler.mInterpolation = vkglTF::AnimationSampler::InterpolationType::LINEAR;
			}
			else if (gltfAnimationSampler.interpolation == "STEP")
			{
				sampler.mInterpolation = vkglTF::AnimationSampler::InterpolationType::STEP;
			}
			else if (gltfAnimationSampler.interpolation == "CUBICSPLINE")
			{
				sampler.mInterpolation = vkglTF::AnimationSampler::InterpolationType::CUBICSPLINE;
			}

			// Read sampler input time values
			{
				const tinygltf::Accessor& gltfAccessor = aGltfModel->accessors[gltfAnimationSampler.input];
				const tinygltf::BufferView& gltfBufferView = aGltfModel->bufferViews[gltfAccessor.bufferView];
				const tinygltf::Buffer& gltfBuffer = aGltfModel->buffers[gltfBufferView.buffer];

				assert(gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				float* buffer = new float[gltfAccessor.count];
				std::memcpy(buffer, &gltfBuffer.data[gltfAccessor.byteOffset + gltfBufferView.byteOffset], gltfAccessor.count * sizeof(float));
				for (size_t index = 0; index < gltfAccessor.count; index++)
				{
					sampler.mInputs.push_back(buffer[index]);
				}

				delete[] buffer;

				for (float input : sampler.mInputs)
				{
					if (input < animation.mStart)
					{
						animation.mStart = input;
					}

					if (input > animation.mEnd)
					{
						animation.mEnd = input;
					}
				}
			}

			// Read sampler output T/R/S values 
			{
				const tinygltf::Accessor& gltfAccessor = aGltfModel->accessors[gltfAnimationSampler.output];
				const tinygltf::BufferView& gltfBufferView = aGltfModel->bufferViews[gltfAccessor.bufferView];
				const tinygltf::Buffer& gltfBuffer = aGltfModel->buffers[gltfBufferView.buffer];

				assert(gltfAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				switch (gltfAccessor.type)
				{
					case TINYGLTF_TYPE_VEC3:
					{
						Math::Vector3f* buffer = new Math::Vector3f[gltfAccessor.count];
						std::memcpy(buffer, &gltfBuffer.data[gltfAccessor.byteOffset + gltfBufferView.byteOffset], gltfAccessor.count * sizeof(Math::Vector3f));
						for (size_t index = 0; index < gltfAccessor.count; index++)
						{
							sampler.mOutputsVec4.push_back(Math::Vector4f(buffer[index], 0.0f));
						}
						delete[] buffer;
						break;
					}
					case TINYGLTF_TYPE_VEC4:
					{
						Math::Vector4f* buf = new Math::Vector4f[gltfAccessor.count];
						std::memcpy(buf, &gltfBuffer.data[gltfAccessor.byteOffset + gltfBufferView.byteOffset], gltfAccessor.count * sizeof(Math::Vector4f));
						for (size_t index = 0; index < gltfAccessor.count; index++)
						{
							sampler.mOutputsVec4.push_back(buf[index]);
						}
						delete[] buf;
						break;
					}
					default:
					{
						std::cout << "unknown type" << std::endl;
						break;
					}
				}
			}

			animation.mSamplers.push_back(sampler);
		}

		// Channels
		for (const tinygltf::AnimationChannel& gltfAnimationChhannel : gltfAnimation.channels)
		{
			vkglTF::AnimationChannel channel{};

			if (gltfAnimationChhannel.target_path == "rotation")
			{
				channel.mPathType = vkglTF::AnimationChannel::PathType::ROTATION;
			}
			else if (gltfAnimationChhannel.target_path == "translation")
			{
				channel.mPathType = vkglTF::AnimationChannel::PathType::TRANSLATION;
			}
			else if (gltfAnimationChhannel.target_path == "scale")
			{
				channel.mPathType = vkglTF::AnimationChannel::PathType::SCALE;
			}
			else if (gltfAnimationChhannel.target_path == "weights")
			{
				std::cout << "weights not yet supported, skipping channel" << std::endl;
				continue;
			}

			channel.mSamplerIndex = gltfAnimationChhannel.sampler;
			channel.mNode = NodeFromIndex(aModel, gltfAnimationChhannel.target_node);
			if (!channel.mNode)
			{
				continue;
			}

			animation.mChannels.push_back(channel);
		}

		aModel.animations.push_back(animation);
	}
}

UniqueIdentifier ModelManager::LoadModel(const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aTransferQueue, FileLoadingFlags aFileLoadingFlags, float aScale)
{
	Time::Timer loadTimer;
	loadTimer.StartTimer();

	tinygltf::TinyGLTF gltfContext;
	if ((aFileLoadingFlags & FileLoadingFlags::DontLoadImages) == FileLoadingFlags::DontLoadImages)
	{
		gltfContext.SetImageLoader(VulkanGlTFModelLocal::LoadImageDataFuncEmpty, nullptr);
	}
	else
	{
		gltfContext.SetImageLoader(VulkanGlTFModelLocal::loadImageDataFunc, nullptr);
	}

	vkglTF::Model* newModel = new vkglTF::Model();
	newModel->path = aPath;
	mVulkanDevice = aDevice;

	std::string error;
	std::string warning;

	tinygltf::Model* sourceGltfModel = new tinygltf::Model();
	const bool isFileLoaded = gltfContext.LoadASCIIFromFile(sourceGltfModel, &error, &warning, aPath.generic_string());
	if (!isFileLoaded)
	{
		throw std::runtime_error(std::format("Could not load glTF file: {} {}", aPath.generic_string(), error));
	}

	if (!warning.empty())
	{
		std::cout << "Warning for " << aPath << ": " << warning << std::endl;
	}

	for (const std::string& extension : sourceGltfModel->extensionsRequired)
	{
		std::cout << " Required extension: " << extension;
	}

	for (const std::string& extension : sourceGltfModel->extensionsUsed)
	{
		std::cout << " Used extension: " << extension;
	}

	std::vector<std::uint32_t> indexBuffer;
	std::vector<vkglTF::Vertex> vertexBuffer;

	if ((aFileLoadingFlags & FileLoadingFlags::DontLoadImages) != FileLoadingFlags::DontLoadImages)
	{
		LoadImages(*newModel, sourceGltfModel);
	}

	LoadMaterials(*newModel, sourceGltfModel);

	const tinygltf::Scene& scene = sourceGltfModel->scenes[sourceGltfModel->defaultScene > -1 ? sourceGltfModel->defaultScene : 0];
	for (size_t i = 0; i < scene.nodes.size(); i++)
	{
		const tinygltf::Node node = sourceGltfModel->nodes[scene.nodes[i]];
		LoadNode(*newModel, sourceGltfModel, nullptr, &node, scene.nodes[i], indexBuffer, vertexBuffer, aScale);
	}

	if (!sourceGltfModel->animations.empty())
	{
		LoadAnimations(*newModel, sourceGltfModel);
	}

	LoadSkins(*newModel, sourceGltfModel);

	for (vkglTF::Node*& node : newModel->linearNodes)
	{
		// Assign skins
		if (node->mSkinIndex > -1)
		{
			node->mSkin = newModel->skins[node->mSkinIndex];
		}

		// Initial pose
		if (node->mMesh)
		{
			node->update();
		}
	}

	// Pre-Calculations for requested features
	const bool preTransform = (aFileLoadingFlags & FileLoadingFlags::PreTransformVertices) == FileLoadingFlags::PreTransformVertices;
	const bool preMultiplyColor = (aFileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors) == FileLoadingFlags::PreMultiplyVertexColors;
	const bool flipY = (aFileLoadingFlags & FileLoadingFlags::FlipY) == FileLoadingFlags::FlipY;
	if (preTransform || preMultiplyColor || flipY)
	{
		for (const vkglTF::Node* node : newModel->linearNodes)
		{
			if (node->mMesh)
			{
				const Math::Matrix4f localMatrix = node->GetMatrix();
				for (const vkglTF::Primitive* primitive : node->mMesh->mPrimitives)
				{
					for (std::uint32_t i = 0; i < primitive->vertexCount; i++)
					{
						vkglTF::Vertex& vertex = vertexBuffer[primitive->firstVertex + i];
						// Pre-transform vertex positions by node-hierarchy
						if (preTransform)
						{
							vertex.mPosition = Math::Vector3f(localMatrix * Math::Vector4f(vertex.mPosition, 1.0f));
							vertex.mNormal = Math::Normalize(Math::Matrix3f(localMatrix) * vertex.mNormal);
						}

						// Flip Y-Axis of vertex positions
						if (flipY)
						{
							vertex.mPosition.y *= -1.0f;
							vertex.mNormal.y *= -1.0f;
						}

						// Pre-Multiply vertex colors with material base color
						if (preMultiplyColor)
						{
							vertex.mColor = primitive->material.mBaseColorFactor * vertex.mColor;
						}
					}
				}
			}
		}
	}

	const size_t vertexBufferSize = vertexBuffer.size() * sizeof(vkglTF::Vertex);
	const size_t indexBufferSize = indexBuffer.size() * sizeof(std::uint32_t);
	CreateBuffers(*newModel, indexBuffer, vertexBuffer, vertexBufferSize, indexBufferSize, aDevice, aTransferQueue);

	GetSceneDimensions(*newModel);

	// Setup descriptors
	std::uint32_t uboCount{0};
	std::uint32_t imageCount{0};
	for (const vkglTF::Node* node : newModel->linearNodes)
	{
		if (node->mMesh)
		{
			uboCount++;
		}
	}

	for (const vkglTF::Material& material : newModel->materials)
	{
		if (material.mBaseColorTexture != nullptr)
		{
			imageCount++;
		}
	}

	CreateDescriptorPool(uboCount, imageCount, aDevice);
	CreateDescriptorSets(*newModel, aDevice);

	loadTimer.EndTimer();

	std::cout << "Loaded GLTF model " << aPath.filename() << " " << std::format("({:.2f}s)", loadTimer.GetDurationSeconds()) << std::endl;

	delete sourceGltfModel;

	UniqueIdentifier identifier{};
	mModels.emplace(identifier, newModel);

	return identifier;
}

vkglTF::Model* ModelManager::GetModel(const UniqueIdentifier aIdentifier) const
{
	return mModels.contains(aIdentifier) ? mModels.at(aIdentifier) : nullptr;
}

void ModelManager::CreateDescriptorSets(vkglTF::Model& aModel, VulkanDevice* aDevice)
{
	// Descriptors for per-node uniform buffers
	{
		// Layout is global, so only create if it hasn't already been created before
		if (vkglTF::gDescriptorSetLayoutUbo == VK_NULL_HANDLE)
		{
			const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};
			const VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &descriptorSetLayoutBinding};
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(aDevice->mLogicalVkDevice, &DescriptorSetLayoutCreateInfo, nullptr, &vkglTF::gDescriptorSetLayoutUbo));
		}

		for (vkglTF::Node*& node : aModel.nodes)
		{
			CreateNodeDescriptorSets(node, vkglTF::gDescriptorSetLayoutUbo);
		}
	}

	// Descriptors for per-material images
	{
		// Layout is global, so only create if it hasn't already been created before
		if (vkglTF::gDescriptorSetLayoutImage == VK_NULL_HANDLE)
		{
			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
			if (vkglTF::gDescriptorBindingFlags & vkglTF::DescriptorBindingFlags::ImageBaseColor)
			{
				setLayoutBindings.push_back({.binding = static_cast<std::uint32_t>(setLayoutBindings.size()), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT});
			}

			if (vkglTF::gDescriptorBindingFlags & vkglTF::DescriptorBindingFlags::ImageNormalMap)
			{
				setLayoutBindings.push_back({.binding = static_cast<std::uint32_t>(setLayoutBindings.size()), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT});
			}

			const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = static_cast<std::uint32_t>(setLayoutBindings.size()),
				.pBindings = setLayoutBindings.data(),
			};
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(aDevice->mLogicalVkDevice, &descriptorSetLayoutCreateInfo, nullptr, &vkglTF::gDescriptorSetLayoutImage));
		}

		for (vkglTF::Material& material : aModel.materials)
		{
			if (material.mBaseColorTexture != nullptr)
			{
				CreateMaterialDescriptorSets(material);
			}
		}
	}
}

void ModelManager::CreateMaterialDescriptorSets(vkglTF::Material& material)
{
	const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &vkglTF::gDescriptorSetLayoutImage,
	};
	VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocateInfo, &material.mDescriptorSet));

	std::vector<VkDescriptorImageInfo> imageDescriptors{};
	std::vector<VkWriteDescriptorSet> writeDescriptorSets{};
	if (vkglTF::gDescriptorBindingFlags & vkglTF::DescriptorBindingFlags::ImageBaseColor)
	{
		imageDescriptors.push_back(material.mBaseColorTexture->mDescriptorImageInfo);

		const VkWriteDescriptorSet writeDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = material.mDescriptorSet,
			.dstBinding = static_cast<Core::uint32>(writeDescriptorSets.size()),
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &material.mBaseColorTexture->mDescriptorImageInfo
		};
		writeDescriptorSets.push_back(writeDescriptorSet);
	}

	if (material.mNormalTexture && vkglTF::gDescriptorBindingFlags & vkglTF::DescriptorBindingFlags::ImageNormalMap)
	{
		imageDescriptors.push_back(material.mNormalTexture->mDescriptorImageInfo);

		const VkWriteDescriptorSet writeDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = material.mDescriptorSet,
			.dstBinding = static_cast<Core::uint32>(writeDescriptorSets.size()),
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &material.mNormalTexture->mDescriptorImageInfo
		};
		writeDescriptorSets.push_back(writeDescriptorSet);
	}

	vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, static_cast<Core::uint32>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void ModelManager::CreateDescriptorPool(uint32_t uboCount, uint32_t imageCount, VulkanDevice* aDevice)
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboCount},
	};

	if (imageCount > 0)
	{
		if (vkglTF::gDescriptorBindingFlags & vkglTF::DescriptorBindingFlags::ImageBaseColor)
		{
			poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount});
		}

		if (vkglTF::gDescriptorBindingFlags & vkglTF::DescriptorBindingFlags::ImageNormalMap)
		{
			poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount});
		}
	}

	const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = uboCount + imageCount,
		.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};
	VK_CHECK_RESULT(vkCreateDescriptorPool(aDevice->mLogicalVkDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool));
}

void ModelManager::CreateBuffers(vkglTF::Model& aModel, std::vector<std::uint32_t>& indexBuffer, std::vector<vkglTF::Vertex>& vertexBuffer, size_t vertexBufferSize, size_t indexBufferSize, VulkanDevice* aDevice, VkQueue aTransferQueue)
{
	aModel.indices.mCount = static_cast<std::uint32_t>(indexBuffer.size());
	aModel.vertices.mCount = static_cast<std::uint32_t>(vertexBuffer.size());

	assert((vertexBufferSize > 0) && (indexBufferSize > 0));

	struct StagingBuffer
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
	};
	StagingBuffer vertexStaging{};
	StagingBuffer indexStaging{};

	// Create staging buffers
	// Vertex data
	VK_CHECK_RESULT(aDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		vertexBufferSize,
		&vertexStaging.buffer,
		&vertexStaging.memory,
		vertexBuffer.data()));

	// Index data
	VK_CHECK_RESULT(aDevice->CreateBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		indexBufferSize,
		&indexStaging.buffer,
		&indexStaging.memory,
		indexBuffer.data()));

	// Create device local buffers
	// Vertex buffer
	VK_CHECK_RESULT(aDevice->CreateBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | vkglTF::gMemoryPropertyFlags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBufferSize,
		&aModel.vertices.mBuffer,
		&aModel.vertices.mMemory, nullptr));

	// Index buffer
	VK_CHECK_RESULT(aDevice->CreateBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | vkglTF::gMemoryPropertyFlags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBufferSize,
		&aModel.indices.mBuffer,
		&aModel.indices.mMemory, nullptr));

	// Copy from staging buffers
	VkCommandBuffer copyCmd = aDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, aModel.vertices.mBuffer, 1, &copyRegion);

	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(copyCmd, indexStaging.buffer, aModel.indices.mBuffer, 1, &copyRegion);

	aDevice->FlushCommandBuffer(copyCmd, aTransferQueue, true);

	vkDestroyBuffer(aDevice->mLogicalVkDevice, vertexStaging.buffer, nullptr);
	vkFreeMemory(aDevice->mLogicalVkDevice, vertexStaging.memory, nullptr);
	vkDestroyBuffer(aDevice->mLogicalVkDevice, indexStaging.buffer, nullptr);
	vkFreeMemory(aDevice->mLogicalVkDevice, indexStaging.memory, nullptr);
}

void ModelManager::GetNodeDimensions(const vkglTF::Node* aNode, Math::Vector3f& aMin, Math::Vector3f& aMax)
{
	if (aNode->mMesh)
	{
		for (const vkglTF::Primitive* primitive : aNode->mMesh->mPrimitives)
		{
			const Math::Vector4f locMin = Math::Vector4f(primitive->mDimensions.mMin, 1.0f) * aNode->GetMatrix();
			const Math::Vector4f locMax = Math::Vector4f(primitive->mDimensions.mMax, 1.0f) * aNode->GetMatrix();
			if (locMin.x < aMin.x) { aMin.x = locMin.x; }
			if (locMin.y < aMin.y) { aMin.y = locMin.y; }
			if (locMin.z < aMin.z) { aMin.z = locMin.z; }
			if (locMax.x > aMax.x) { aMax.x = locMax.x; }
			if (locMax.y > aMax.y) { aMax.y = locMax.y; }
			if (locMax.z > aMax.z) { aMax.z = locMax.z; }
		}
	}

	for (const vkglTF::Node* child : aNode->mChildren)
	{
		GetNodeDimensions(child, aMin, aMax);
	}
}

void ModelManager::GetSceneDimensions(vkglTF::Model& aModel)
{
	aModel.mDimensions.mMin = Math::Vector3f(std::numeric_limits<float>::max());
	aModel.mDimensions.mMax = Math::Vector3f(std::numeric_limits<float>::lowest());

	for (vkglTF::Node*& node : aModel.nodes)
	{
		GetNodeDimensions(node, aModel.mDimensions.mMin, aModel.mDimensions.mMax);
	}

	aModel.mDimensions.mSize = aModel.mDimensions.mMax - aModel.mDimensions.mMin;
	aModel.mDimensions.mCenter = (aModel.mDimensions.mMin + aModel.mDimensions.mMax) / 2.0f;
	aModel.mDimensions.mRadius = Math::Distance(aModel.mDimensions.mMin, aModel.mDimensions.mMax) / 2.0f;
}

void ModelManager::UpdateAnimation(vkglTF::Model& aModel, std::uint32_t aIndex, float aTime)

{
	if (aIndex > static_cast<std::uint32_t>(aModel.animations.size()) - 1)
	{
		std::cout << "No animation with index " << aIndex << std::endl;
		return;
	}

	vkglTF::Animation& animation = aModel.animations[aIndex];

	bool updated = false;
	for (vkglTF::AnimationChannel& channel : animation.mChannels)
	{
		vkglTF::AnimationSampler& sampler = animation.mSamplers[channel.mSamplerIndex];
		if (sampler.mInputs.size() > sampler.mOutputsVec4.size())
		{
			continue;
		}

		for (size_t i = 0; i < sampler.mInputs.size() - 1; i++)
		{
			if ((aTime >= sampler.mInputs[i]) && (aTime <= sampler.mInputs[i + 1]))
			{
				float u = std::max(0.0f, aTime - sampler.mInputs[i]) / (sampler.mInputs[i + 1] - sampler.mInputs[i]);
				if (u <= 1.0f)
				{
					switch (channel.mPathType)
					{
						case vkglTF::AnimationChannel::PathType::TRANSLATION:
						{
							Math::Vector4f trans = Math::Mix(sampler.mOutputsVec4[i], sampler.mOutputsVec4[i + 1], u);
							channel.mNode->mTranslation = Math::Vector3f(trans);
							break;
						}
						case vkglTF::AnimationChannel::PathType::SCALE:
						{
							Math::Vector4f trans = Math::Mix(sampler.mOutputsVec4[i], sampler.mOutputsVec4[i + 1], u);
							channel.mNode->mScale = Math::Vector3f(trans);
							break;
						}
						case vkglTF::AnimationChannel::PathType::ROTATION:
						{
							Math::Quaternionf q1{};
							q1.x = sampler.mOutputsVec4[i].x;
							q1.y = sampler.mOutputsVec4[i].y;
							q1.z = sampler.mOutputsVec4[i].z;
							q1.w = sampler.mOutputsVec4[i].w;

							Math::Quaternionf q2{};
							q2.x = sampler.mOutputsVec4[i + 1].x;
							q2.y = sampler.mOutputsVec4[i + 1].y;
							q2.z = sampler.mOutputsVec4[i + 1].z;
							q2.w = sampler.mOutputsVec4[i + 1].w;
							channel.mNode->mRotation = Math::Normalize(Math::Slerp(q1, q2, u));
							break;
						}
					}
					updated = true;
				}
			}
		}
	}

	if (updated)
	{
		for (vkglTF::Node*& node : aModel.nodes)
		{
			node->update();
		}
	}
}

vkglTF::Node* ModelManager::FindNode(vkglTF::Node* aParent, std::uint32_t aIndex)
{
	vkglTF::Node* nodeFound = nullptr;
	if (aParent->mIndex == aIndex)
	{
		return aParent;
	}

	for (vkglTF::Node*& child : aParent->mChildren)
	{
		nodeFound = FindNode(child, aIndex);
		if (nodeFound)
		{
			break;
		}
	}

	return nodeFound;
}

vkglTF::Node* ModelManager::NodeFromIndex(vkglTF::Model& aModel, std::uint32_t aIndex)
{
	vkglTF::Node* nodeFound = nullptr;
	for (vkglTF::Node*& node : aModel.nodes)
	{
		nodeFound = FindNode(node, aIndex);
		if (nodeFound)
		{
			break;
		}
	}

	return nodeFound;
}

void ModelManager::CreateNodeDescriptorSets(vkglTF::Node* aNode, const VkDescriptorSetLayout aDescriptorSetLayout)
{
	if (aNode->mMesh)
	{
		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &aDescriptorSetLayout
		};
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocateInfo, &aNode->mMesh->mUniformBuffer.mDescriptorSet));

		const VkWriteDescriptorSet writeDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = aNode->mMesh->mUniformBuffer.mDescriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &aNode->mMesh->mUniformBuffer.descriptor
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, 1, &writeDescriptorSet, 0, nullptr);
	}

	for (vkglTF::Node*& child : aNode->mChildren)
	{
		CreateNodeDescriptorSets(child, aDescriptorSetLayout);
	}
}
