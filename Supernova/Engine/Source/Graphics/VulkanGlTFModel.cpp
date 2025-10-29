#include "VulkanglTFModel.hpp"

#include "Math/Functions.hpp"
#include "Math/Types.hpp"
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

vkglTF::Model::Model()
	: mCurrentModel{nullptr}
	, mVulkanDevice{nullptr}
	, descriptorPool{VK_NULL_HANDLE}
	, metallicRoughnessWorkflow{false}
	, buffersBound{false}
{
}

vkglTF::Model::~Model()
{
	vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, vertices.mBuffer, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, vertices.mMemory, nullptr);
	vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, indices.mBuffer, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, indices.mMemory, nullptr);
	for (vkglTF::Texture& texture : textures)
	{
		texture.Destroy();
	}

	for (vkglTF::Node*& node : nodes)
	{
		delete node;
	}

	for (vkglTF::Skin*& skin : skins)
	{
		delete skin;
	}

	if (gDescriptorSetLayoutUbo != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, gDescriptorSetLayoutUbo, nullptr);
		gDescriptorSetLayoutUbo = VK_NULL_HANDLE;
	}

	if (gDescriptorSetLayoutImage != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, gDescriptorSetLayoutImage, nullptr);
		gDescriptorSetLayoutImage = VK_NULL_HANDLE;
	}

	vkDestroyDescriptorPool(mVulkanDevice->mLogicalVkDevice, descriptorPool, nullptr);
	mEmptyTexture.Destroy();
}

void vkglTF::Model::LoadNode(vkglTF::Node* aParent, const tinygltf::Node* aNode, std::uint32_t aNodeIndex, std::vector<std::uint32_t>& aIndexBuffer, std::vector<Vertex>& aVertexBuffer, float aGlobalscale)
{
	vkglTF::Node* newNode = new Node{};
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
            LoadNode(newNode, &mCurrentModel->nodes[aNode->children[i]], aNode->children[i], aIndexBuffer, aVertexBuffer, aGlobalscale);
		}
	}

	// Node contains mesh data
    if (aNode->mesh > -1)
	{
		const tinygltf::Mesh& mesh = mCurrentModel->meshes[aNode->mesh];
		Mesh* newMesh = new Mesh(mVulkanDevice, newNode->mMatrix);
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

                const tinygltf::Accessor& posAccessor = mCurrentModel->accessors[primitive.attributes.find("POSITION")->second];
                const tinygltf::BufferView& posView = mCurrentModel->bufferViews[posAccessor.bufferView];
                bufferPos = reinterpret_cast<const float*>(&(mCurrentModel->buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
				posMin = Math::Vector3f(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
				posMax = Math::Vector3f(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);

				if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
				{
                    const tinygltf::Accessor& normAccessor = mCurrentModel->accessors[primitive.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView& normView = mCurrentModel->bufferViews[normAccessor.bufferView];
                    bufferNormals = reinterpret_cast<const float*>(&(mCurrentModel->buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
				}

				if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
				{
                    const tinygltf::Accessor& uvAccessor = mCurrentModel->accessors[primitive.attributes.find("TEXCOORD_0")->second];
                    const tinygltf::BufferView& uvView = mCurrentModel->bufferViews[uvAccessor.bufferView];
                    bufferTexCoords = reinterpret_cast<const float*>(&(mCurrentModel->buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
				}

				if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
				{
                    const tinygltf::Accessor& colorAccessor = mCurrentModel->accessors[primitive.attributes.find("COLOR_0")->second];
                    const tinygltf::BufferView& colorView = mCurrentModel->bufferViews[colorAccessor.bufferView];
					// Color buffer are either of type vec3 or vec4
					numColorComponents = colorAccessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
                    bufferColors = reinterpret_cast<const float*>(&(mCurrentModel->buffers[colorView.buffer].data[colorAccessor.byteOffset + colorView.byteOffset]));
				}

				if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
				{
                    const tinygltf::Accessor& tangentAccessor = mCurrentModel->accessors[primitive.attributes.find("TANGENT")->second];
                    const tinygltf::BufferView& tangentView = mCurrentModel->bufferViews[tangentAccessor.bufferView];
                    bufferTangents = reinterpret_cast<const float*>(&(mCurrentModel->buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]));
				}

				// Skinning
				// Joints
				if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
				{
                    const tinygltf::Accessor& jointAccessor = mCurrentModel->accessors[primitive.attributes.find("JOINTS_0")->second];
                    const tinygltf::BufferView& jointView = mCurrentModel->bufferViews[jointAccessor.bufferView];
                    bufferJoints = reinterpret_cast<const uint16_t*>(&(mCurrentModel->buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]));
				}

				if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
				{
                    const tinygltf::Accessor& uvAccessor = mCurrentModel->accessors[primitive.attributes.find("WEIGHTS_0")->second];
                    const tinygltf::BufferView& uvView = mCurrentModel->bufferViews[uvAccessor.bufferView];
                    bufferWeights = reinterpret_cast<const float*>(&(mCurrentModel->buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
				}

				hasSkin = (bufferJoints && bufferWeights);

				vertexCount = static_cast<std::uint32_t>(posAccessor.count);

				for (size_t v = 0; v < posAccessor.count; v++)
				{
					Vertex vert{};
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
                const tinygltf::Accessor& accessor = mCurrentModel->accessors[primitive.indices];
                const tinygltf::BufferView& bufferView = mCurrentModel->bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = mCurrentModel->buffers[bufferView.buffer];

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
			Primitive* newPrimitive = new Primitive(indexStart, indexCount, primitive.material > -1 ? materials[primitive.material] : materials.back());
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
		nodes.push_back(newNode);
	}
	linearNodes.push_back(newNode);
}

void vkglTF::Model::LoadSkins()
{
    for (const tinygltf::Skin& source : mCurrentModel->skins)
	{
		Skin* newSkin = new Skin{};
		newSkin->mName = source.name;

		// Find skeleton root node
		if (source.skeleton > -1)
		{
			newSkin->mSkeletonRoot = NodeFromIndex(source.skeleton);
		}

		// Find joint nodes
		for (int jointIndex : source.joints)
		{
			Node* node = NodeFromIndex(jointIndex);
			if (node)
			{
				newSkin->joints.push_back(NodeFromIndex(jointIndex));
			}
		}

		// Get inverse bind matrices from buffer
		if (source.inverseBindMatrices > -1)
		{
            const tinygltf::Accessor& accessor = mCurrentModel->accessors[source.inverseBindMatrices];
            const tinygltf::BufferView& bufferView = mCurrentModel->bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = mCurrentModel->buffers[bufferView.buffer];
			newSkin->inverseBindMatrices.resize(accessor.count);
			std::memcpy(newSkin->inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(Math::Matrix4f));
		}

		skins.push_back(newSkin);
	}
}

void vkglTF::Model::LoadImages(VulkanDevice* aDevice, VkQueue aTransferQueue)
{
	for (tinygltf::Image& image : mCurrentModel->images)
	{
		vkglTF::Texture texture;
		texture.CreateTexture(&image, path, aDevice, aTransferQueue);
		texture.mIndex = static_cast<std::uint32_t>(textures.size());
		textures.push_back(texture);
	}
	// Create an empty texture to be used for empty material images
	CreateEmptyTexture(aTransferQueue);
}

void vkglTF::Model::LoadMaterials()
{
    for (tinygltf::Material& mat : mCurrentModel->materials)
	{
		vkglTF::Material material(mVulkanDevice);
		if (mat.values.find("baseColorTexture") != mat.values.end())
		{
            material.mBaseColorTexture = GetTexture(mCurrentModel->textures[mat.values["baseColorTexture"].TextureIndex()].source);
		}
		// Metallic roughness workflow
		if (mat.values.find("metallicRoughnessTexture") != mat.values.end())
		{
            material.mMetallicRoughnessTexture = GetTexture(mCurrentModel->textures[mat.values["metallicRoughnessTexture"].TextureIndex()].source);
		}
		if (mat.values.find("roughnessFactor") != mat.values.end())
		{
			material.mRoughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
		}
		if (mat.values.find("metallicFactor") != mat.values.end())
		{
			material.mMetallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
		}
		if (mat.values.find("baseColorFactor") != mat.values.end())
		{
			material.mBaseColorFactor = Math::MakeVector4f(mat.values["baseColorFactor"].ColorFactor().data());
		}
		if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end())
		{
            material.mNormalTexture = GetTexture(mCurrentModel->textures[mat.additionalValues["normalTexture"].TextureIndex()].source);
		}
		else
		{
			material.mNormalTexture = &mEmptyTexture;
		}
		if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end())
		{
            material.mEmissiveTexture = GetTexture(mCurrentModel->textures[mat.additionalValues["emissiveTexture"].TextureIndex()].source);
		}
		if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end())
		{
            material.mOcclusionTexture = GetTexture(mCurrentModel->textures[mat.additionalValues["occlusionTexture"].TextureIndex()].source);
		}
		if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end())
		{
			const tinygltf::Parameter param = mat.additionalValues["alphaMode"];
			if (param.string_value == "BLEND")
			{
				material.mAlphaMode = Material::AlphaMode::ALPHAMODE_BLEND;
			}
			if (param.string_value == "MASK")
			{
				material.mAlphaMode = Material::AlphaMode::ALPHAMODE_MASK;
			}
		}
		if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end())
		{
			material.mAlphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
		}

		materials.push_back(material);
	}
	// Push a default material at the end of the list for meshes with no material assigned
	materials.push_back(Material(mVulkanDevice));
}

void vkglTF::Model::LoadAnimations()
{
    for (const tinygltf::Animation& anim : mCurrentModel->animations)
	{
		vkglTF::Animation animation{};
		animation.mName = anim.name;
		if (anim.name.empty())
		{
			animation.mName = std::to_string(animations.size());
		}

		// Samplers
		for (const tinygltf::AnimationSampler& samp : anim.samplers)
		{
			vkglTF::AnimationSampler sampler{};

			if (samp.interpolation == "LINEAR")
			{
				sampler.mInterpolation = AnimationSampler::InterpolationType::LINEAR;
			}
			if (samp.interpolation == "STEP")
			{
				sampler.mInterpolation = AnimationSampler::InterpolationType::STEP;
			}
			if (samp.interpolation == "CUBICSPLINE")
			{
				sampler.mInterpolation = AnimationSampler::InterpolationType::CUBICSPLINE;
			}

			// Read sampler input time values
			{
                const tinygltf::Accessor& accessor = mCurrentModel->accessors[samp.input];
                const tinygltf::BufferView& bufferView = mCurrentModel->bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = mCurrentModel->buffers[bufferView.buffer];

				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				float* buf = new float[accessor.count];
				std::memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(float));
				for (size_t index = 0; index < accessor.count; index++)
				{
					sampler.mInputs.push_back(buf[index]);
				}

				delete[] buf;

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
                const tinygltf::Accessor& accessor = mCurrentModel->accessors[samp.output];
                const tinygltf::BufferView& bufferView = mCurrentModel->bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = mCurrentModel->buffers[bufferView.buffer];

				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				switch (accessor.type)
				{
					case TINYGLTF_TYPE_VEC3:
					{
						Math::Vector3f* buf = new Math::Vector3f[accessor.count];
						std::memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(Math::Vector3f));
						for (size_t index = 0; index < accessor.count; index++)
						{
							sampler.mOutputsVec4.push_back(Math::Vector4f(buf[index], 0.0f));
						}
						delete[] buf;
						break;
					}
					case TINYGLTF_TYPE_VEC4:
					{
						Math::Vector4f* buf = new Math::Vector4f[accessor.count];
						std::memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(Math::Vector4f));
						for (size_t index = 0; index < accessor.count; index++)
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
		for (const tinygltf::AnimationChannel& source : anim.channels)
		{
			vkglTF::AnimationChannel channel{};

			if (source.target_path == "rotation")
			{
				channel.mPathType = AnimationChannel::PathType::ROTATION;
			}
			if (source.target_path == "translation")
			{
				channel.mPathType = AnimationChannel::PathType::TRANSLATION;
			}
			if (source.target_path == "scale")
			{
				channel.mPathType = AnimationChannel::PathType::SCALE;
			}
			if (source.target_path == "weights")
			{
				std::cout << "weights not yet supported, skipping channel" << std::endl;
				continue;
			}
			channel.mSamplerIndex = source.sampler;
			channel.mNode = NodeFromIndex(source.target_node);
			if (!channel.mNode)
			{
				continue;
			}

			animation.mChannels.push_back(channel);
		}

		animations.push_back(animation);
	}
}

void vkglTF::Model::LoadFromFile(const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aTransferQueue, std::uint32_t aFileLoadingFlags, float aScale)
{
	tinygltf::TinyGLTF gltfContext;
	if (aFileLoadingFlags & FileLoadingFlags::DontLoadImages)
	{
		gltfContext.SetImageLoader(VulkanGlTFModelLocal::LoadImageDataFuncEmpty, nullptr);
	}
	else
	{
		gltfContext.SetImageLoader(VulkanGlTFModelLocal::loadImageDataFunc, nullptr);
	}

	const std::string pathString = aPath.generic_string();
	size_t pos = pathString.find_last_of('/');
	path = pathString.substr(0, pos);

	std::string error;
	std::string warning;

	mVulkanDevice = aDevice;

	mCurrentModel = new tinygltf::Model();
	const bool isFileLoaded = gltfContext.LoadASCIIFromFile(mCurrentModel, &error, &warning, pathString);
	if (!isFileLoaded)
	{
		throw std::runtime_error(std::format("Could not load glTF file: {} {}", aPath.generic_string(), error));
	}

	std::vector<std::uint32_t> indexBuffer;
	std::vector<Vertex> vertexBuffer;

	if (!(aFileLoadingFlags & FileLoadingFlags::DontLoadImages))
	{
		LoadImages(aDevice, aTransferQueue);
	}

    LoadMaterials();

	const tinygltf::Scene& scene = mCurrentModel->scenes[mCurrentModel->defaultScene > -1 ? mCurrentModel->defaultScene : 0];
	for (size_t i = 0; i < scene.nodes.size(); i++)
	{
		const tinygltf::Node node = mCurrentModel->nodes[scene.nodes[i]];
        LoadNode(nullptr, &node, scene.nodes[i], indexBuffer, vertexBuffer, aScale);
	}

	if (!mCurrentModel->animations.empty())
	{
        LoadAnimations();
	}

    LoadSkins();

	for (vkglTF::Node*& node : linearNodes)
	{
		// Assign skins
		if (node->mSkinIndex > -1)
		{
			node->mSkin = skins[node->mSkinIndex];
		}

		// Initial pose
		if (node->mMesh)
		{
			node->update();
		}
	}

	// Pre-Calculations for requested features
	if ((aFileLoadingFlags & FileLoadingFlags::PreTransformVertices) || (aFileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors) || (aFileLoadingFlags & FileLoadingFlags::FlipY))
	{
		const bool preTransform = aFileLoadingFlags & FileLoadingFlags::PreTransformVertices;
		const bool preMultiplyColor = aFileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors;
		const bool flipY = aFileLoadingFlags & FileLoadingFlags::FlipY;
		for (const Node* node : linearNodes)
		{
			if (node->mMesh)
			{
				const Math::Matrix4f localMatrix = node->GetMatrix();
				for (const Primitive* primitive : node->mMesh->mPrimitives)
				{
					for (std::uint32_t i = 0; i < primitive->vertexCount; i++)
					{
						Vertex& vertex = vertexBuffer[primitive->firstVertex + i];
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

	for (const std::string& extension : mCurrentModel->extensionsUsed)
	{
		if (extension == "KHR_materials_pbrSpecularGlossiness")
		{
			std::cout << "Required extension: " << extension;
			metallicRoughnessWorkflow = false;
		}
	}

	size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);
	size_t indexBufferSize = indexBuffer.size() * sizeof(std::uint32_t);
	indices.mCount = static_cast<std::uint32_t>(indexBuffer.size());
	vertices.mCount = static_cast<std::uint32_t>(vertexBuffer.size());

	assert((vertexBufferSize > 0) && (indexBufferSize > 0));

	struct StagingBuffer
	{
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertexStaging{}, indexStaging{};

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
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | gMemoryPropertyFlags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBufferSize,
		&vertices.mBuffer,
		&vertices.mMemory, nullptr));

	// Index buffer
	VK_CHECK_RESULT(aDevice->CreateBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | gMemoryPropertyFlags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBufferSize,
		&indices.mBuffer,
		&indices.mMemory, nullptr));

	// Copy from staging buffers
	VkCommandBuffer copyCmd = aDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertices.mBuffer, 1, &copyRegion);

	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indices.mBuffer, 1, &copyRegion);

	aDevice->FlushCommandBuffer(copyCmd, aTransferQueue, true);

	vkDestroyBuffer(aDevice->mLogicalVkDevice, vertexStaging.buffer, nullptr);
	vkFreeMemory(aDevice->mLogicalVkDevice, vertexStaging.memory, nullptr);
	vkDestroyBuffer(aDevice->mLogicalVkDevice, indexStaging.buffer, nullptr);
	vkFreeMemory(aDevice->mLogicalVkDevice, indexStaging.memory, nullptr);

	GetSceneDimensions();

	// Setup descriptors
	std::uint32_t uboCount{0};
	std::uint32_t imageCount{0};
	for (const vkglTF::Node* node : linearNodes)
	{
		if (node->mMesh)
		{
			uboCount++;
		}
	}

	for (const vkglTF::Material& material : materials)
	{
		if (material.mBaseColorTexture != nullptr)
		{
			imageCount++;
		}
	}

	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboCount },
	};

	if (imageCount > 0)
	{
		if (gDescriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
		{
			poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount});
		}

		if (gDescriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap)
		{
			poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount});
		}
	}

	VkDescriptorPoolCreateInfo descriptorPoolCI{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = uboCount + imageCount,
		.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};
	VK_CHECK_RESULT(vkCreateDescriptorPool(aDevice->mLogicalVkDevice, &descriptorPoolCI, nullptr, &descriptorPool));

	// Descriptors for per-node uniform buffers
	{
		// Layout is global, so only create if it hasn't already been created before
		if (gDescriptorSetLayoutUbo == VK_NULL_HANDLE)
		{
			VkDescriptorSetLayoutBinding setLayoutBinding{.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};
			VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &setLayoutBinding};
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(aDevice->mLogicalVkDevice, &descriptorLayoutCI, nullptr, &gDescriptorSetLayoutUbo));
		}

		for (vkglTF::Node*& node : nodes)
		{
			PrepareNodeDescriptor(node, gDescriptorSetLayoutUbo);
		}
	}

	// Descriptors for per-material images
	{
		// Layout is global, so only create if it hasn't already been created before
		if (gDescriptorSetLayoutImage == VK_NULL_HANDLE)
		{
			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
			if (gDescriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
			{
				setLayoutBindings.push_back({.binding = static_cast<std::uint32_t>(setLayoutBindings.size()), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT});
			}

			if (gDescriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap)
			{
				setLayoutBindings.push_back({.binding = static_cast<std::uint32_t>(setLayoutBindings.size()), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT});
			}

			VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = static_cast<std::uint32_t>(setLayoutBindings.size()),
				.pBindings = setLayoutBindings.data(),
			};
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(aDevice->mLogicalVkDevice, &descriptorLayoutCI, nullptr, &gDescriptorSetLayoutImage));
		}

		for (vkglTF::Material& material : materials)
		{
			if (material.mBaseColorTexture != nullptr)
			{
				material.CreateDescriptorSet(descriptorPool, vkglTF::gDescriptorSetLayoutImage, gDescriptorBindingFlags);
			}
		}
	}

	std::cout << "Loaded GLTF model " << aPath.filename() << std::endl;

	delete mCurrentModel;
}

vkglTF::Texture* vkglTF::Model::GetTexture(std::uint32_t aIndex)
{
	if (aIndex < textures.size())
		return &textures[aIndex];

	return nullptr;
}

void vkglTF::Model::CreateEmptyTexture(VkQueue aTransferQueue)
{
	mEmptyTexture.mVulkanDevice = mVulkanDevice;
	mEmptyTexture.mWidth = 1;
	mEmptyTexture.mHeight = 1;
	mEmptyTexture.mLayerCount = 1;
	mEmptyTexture.mMipLevels = 1;

	size_t bufferSize = static_cast<std::size_t>(mEmptyTexture.mWidth * mEmptyTexture.mHeight * 4);
	unsigned char* buffer = new unsigned char[bufferSize];
	memset(buffer, 0, bufferSize);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	VkBufferCreateInfo bufferCreateInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = bufferSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	};
	VK_CHECK_RESULT(vkCreateBuffer(mVulkanDevice->mLogicalVkDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(mVulkanDevice->mLogicalVkDevice, stagingBuffer, &memReqs);
	VkMemoryAllocateInfo memAllocInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memReqs.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(mVulkanDevice->mLogicalVkDevice, stagingBuffer, stagingMemory, 0));

	// Copy texture data into staging buffer
	uint8_t* data{nullptr};
	VK_CHECK_RESULT(vkMapMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	std::memcpy(data, buffer, bufferSize);
	vkUnmapMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory);

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {.width = mEmptyTexture.mWidth, .height = mEmptyTexture.mHeight, .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &mEmptyTexture.mImage));

	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalVkDevice, mEmptyTexture.mImage, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &memAllocInfo, nullptr, &mEmptyTexture.mDeviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalVkDevice, mEmptyTexture.mImage, mEmptyTexture.mDeviceMemory, 0));

	VkBufferImageCopy bufferCopyRegion{
		.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
		.imageExtent = {.width = mEmptyTexture.mWidth, .height = mEmptyTexture.mHeight, .depth = 1 }
	};
	VkImageSubresourceRange subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .layerCount = 1};
	VkCommandBuffer copyCmd = mVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VulkanTools::SetImageLayout(copyCmd, mEmptyTexture.mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
	vkCmdCopyBufferToImage(copyCmd, stagingBuffer, mEmptyTexture.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
	VulkanTools::SetImageLayout(copyCmd, mEmptyTexture.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
	mVulkanDevice->FlushCommandBuffer(copyCmd, aTransferQueue, true);
	mEmptyTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Clean up staging resources
	vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, stagingBuffer, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, stagingMemory, nullptr);

	VkSamplerCreateInfo samplerCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.maxAnisotropy = 1.0f,
		.compareOp = VK_COMPARE_OP_NEVER,
	};
	VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->mLogicalVkDevice, &samplerCreateInfo, nullptr, &mEmptyTexture.mSampler));

	VkImageViewCreateInfo viewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = mEmptyTexture.mImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 },
	};
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalVkDevice, &viewCreateInfo, nullptr, &mEmptyTexture.mImageView));

	mEmptyTexture.mDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	mEmptyTexture.mDescriptorImageInfo.imageView = mEmptyTexture.mImageView;
	mEmptyTexture.mDescriptorImageInfo.sampler = mEmptyTexture.mSampler;
}

void vkglTF::Model::BindBuffers(VkCommandBuffer aCommandBuffer)
{
	const VkDeviceSize offsets[1] = {0};
	vkCmdBindVertexBuffers(aCommandBuffer, 0, 1, &vertices.mBuffer, offsets);
	vkCmdBindIndexBuffer(aCommandBuffer, indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
	buffersBound = true;
}

void vkglTF::Model::GetNodeDimensions(const Node* aNode, Math::Vector3f& aMin, Math::Vector3f& aMax)
{
	if (aNode->mMesh)
	{
		for (const Primitive* primitive : aNode->mMesh->mPrimitives)
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

void vkglTF::Model::GetSceneDimensions()
{
	mDimensions.mMin = Math::Vector3f(std::numeric_limits<float>::max());
	mDimensions.mMax = Math::Vector3f(std::numeric_limits<float>::lowest());
	for (vkglTF::Node*& node : nodes)
	{
		GetNodeDimensions(node, mDimensions.mMin, mDimensions.mMax);
	}
	mDimensions.mSize = mDimensions.mMax - mDimensions.mMin;
	mDimensions.mCenter = (mDimensions.mMin + mDimensions.mMax) / 2.0f;
	mDimensions.mRadius = Math::Distance(mDimensions.mMin, mDimensions.mMax) / 2.0f;
}

void vkglTF::Model::UpdateAnimation(std::uint32_t aIndex, float aTime)

{
	if (aIndex > static_cast<std::uint32_t>(animations.size()) - 1)
	{
		std::cout << "No animation with index " << aIndex << std::endl;
		return;
	}

	Animation& animation = animations[aIndex];

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
		for (vkglTF::Node*& node : nodes)
		{
			node->update();
		}
	}
}

/*
	Helper functions
*/
vkglTF::Node* vkglTF::Model::FindNode(Node* aParent, std::uint32_t aIndex)
{
	Node* nodeFound = nullptr;
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

vkglTF::Node* vkglTF::Model::NodeFromIndex(std::uint32_t aIndex)
{
	Node* nodeFound = nullptr;
	for (vkglTF::Node*& node : nodes)
	{
		nodeFound = FindNode(node, aIndex);
		if (nodeFound)
		{
			break;
		}
	}

	return nodeFound;
}

void vkglTF::Model::PrepareNodeDescriptor(vkglTF::Node* aNode, VkDescriptorSetLayout aDescriptorSetLayout)
{
	if (aNode->mMesh)
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &aDescriptorSetLayout
		};
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocInfo, &aNode->mMesh->mUniformBuffer.mDescriptorSet));

		VkWriteDescriptorSet writeDescriptorSet{
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
		PrepareNodeDescriptor(child, aDescriptorSetLayout);
	}
}
