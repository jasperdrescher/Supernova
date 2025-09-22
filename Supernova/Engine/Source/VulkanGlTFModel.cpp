#include "VulkanglTFModel.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTools.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
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

VkDescriptorSetLayout vkglTF::descriptorSetLayoutImage = VK_NULL_HANDLE;
VkDescriptorSetLayout vkglTF::descriptorSetLayoutUbo = VK_NULL_HANDLE;
VkMemoryPropertyFlags vkglTF::memoryPropertyFlags = 0;
std::uint32_t vkglTF::descriptorBindingFlags = vkglTF::DescriptorBindingFlags::ImageBaseColor;

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

vkglTF::Model::~Model()
{
	vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, vertices.buffer, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, vertices.memory, nullptr);
	vkDestroyBuffer(mVulkanDevice->mLogicalVkDevice, indices.buffer, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, indices.memory, nullptr);
	for (auto& texture : textures)
	{
		texture.Destroy();
	}
	for (auto& node : nodes)
	{
		delete node;
	}
	for (auto& skin : skins)
	{
		delete skin;
	}
	if (descriptorSetLayoutUbo != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, descriptorSetLayoutUbo, nullptr);
		descriptorSetLayoutUbo = VK_NULL_HANDLE;
	}
	if (descriptorSetLayoutImage != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, descriptorSetLayoutImage, nullptr);
		descriptorSetLayoutImage = VK_NULL_HANDLE;
	}
	vkDestroyDescriptorPool(mVulkanDevice->mLogicalVkDevice, descriptorPool, nullptr);
	mEmptyTexture.Destroy();
}

void vkglTF::Model::loadNode(vkglTF::Node* parent, const tinygltf::Node& node, std::uint32_t nodeIndex, const tinygltf::Model& model, std::vector<std::uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalscale)
{
	vkglTF::Node* newNode = new Node{};
	newNode->index = nodeIndex;
	newNode->parent = parent;
	newNode->name = node.name;
	newNode->skinIndex = node.skin;
	newNode->matrix = glm::mat4(1.0f);

	// Generate local node matrix
	glm::vec3 translation = glm::vec3(0.0f);
	if (node.translation.size() == 3)
	{
		translation = glm::make_vec3(node.translation.data());
		newNode->translation = translation;
	}
	glm::mat4 rotation = glm::mat4(1.0f);
	if (node.rotation.size() == 4)
	{
		glm::quat q = glm::make_quat(node.rotation.data());
		newNode->rotation = glm::mat4(q);
	}
	glm::vec3 scale = glm::vec3(1.0f);
	if (node.scale.size() == 3)
	{
		scale = glm::make_vec3(node.scale.data());
		newNode->scale = scale;
	}
	if (node.matrix.size() == 16)
	{
		newNode->matrix = glm::make_mat4x4(node.matrix.data());
		if (globalscale != 1.0f)
		{
			//newNode->matrix = glm::scale(newNode->matrix, glm::vec3(globalscale));
		}
	};

	// Node with children
	if (node.children.size() > 0)
	{
		for (auto i = 0; i < node.children.size(); i++)
		{
			loadNode(newNode, model.nodes[node.children[i]], node.children[i], model, indexBuffer, vertexBuffer, globalscale);
		}
	}

	// Node contains mesh data
	if (node.mesh > -1)
	{
		const tinygltf::Mesh mesh = model.meshes[node.mesh];
		Mesh* newMesh = new Mesh(mVulkanDevice, newNode->matrix);
		newMesh->name = mesh.name;
		for (size_t j = 0; j < mesh.primitives.size(); j++)
		{
			const tinygltf::Primitive& primitive = mesh.primitives[j];
			if (primitive.indices < 0)
			{
				continue;
			}
			std::uint32_t indexStart = static_cast<std::uint32_t>(indexBuffer.size());
			std::uint32_t vertexStart = static_cast<std::uint32_t>(vertexBuffer.size());
			std::uint32_t indexCount = 0;
			std::uint32_t vertexCount = 0;
			glm::vec3 posMin{};
			glm::vec3 posMax{};
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

				const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
				const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
				bufferPos = reinterpret_cast<const float*>(&(model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
				posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
				posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);

				if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
				{
					const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
					bufferNormals = reinterpret_cast<const float*>(&(model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
				}

				if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
				{
					const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
					bufferTexCoords = reinterpret_cast<const float*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
				}

				if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
				{
					const tinygltf::Accessor& colorAccessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
					const tinygltf::BufferView& colorView = model.bufferViews[colorAccessor.bufferView];
					// Color buffer are either of type vec3 or vec4
					numColorComponents = colorAccessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
					bufferColors = reinterpret_cast<const float*>(&(model.buffers[colorView.buffer].data[colorAccessor.byteOffset + colorView.byteOffset]));
				}

				if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
				{
					const tinygltf::Accessor& tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView& tangentView = model.bufferViews[tangentAccessor.bufferView];
					bufferTangents = reinterpret_cast<const float*>(&(model.buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]));
				}

				// Skinning
				// Joints
				if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
				{
					const tinygltf::Accessor& jointAccessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
					const tinygltf::BufferView& jointView = model.bufferViews[jointAccessor.bufferView];
					bufferJoints = reinterpret_cast<const uint16_t*>(&(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]));
				}

				if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
				{
					const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
					const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
					bufferWeights = reinterpret_cast<const float*>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
				}

				hasSkin = (bufferJoints && bufferWeights);

				vertexCount = static_cast<std::uint32_t>(posAccessor.count);

				for (size_t v = 0; v < posAccessor.count; v++)
				{
					Vertex vert{};
					vert.pos = glm::vec4(glm::make_vec3(&bufferPos[v * 3]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * 3]) : glm::vec3(0.0f)));
					vert.uv = bufferTexCoords ? glm::make_vec2(&bufferTexCoords[v * 2]) : glm::vec3(0.0f);
					if (bufferColors)
					{
						switch (numColorComponents)
						{
							case 3:
								vert.color = glm::vec4(glm::make_vec3(&bufferColors[v * 3]), 1.0f);
								break;
							case 4:
								vert.color = glm::make_vec4(&bufferColors[v * 4]);
								break;
						}
					}
					else
					{
						vert.color = glm::vec4(1.0f);
					}
					vert.tangent = bufferTangents ? glm::vec4(glm::make_vec4(&bufferTangents[v * 4])) : glm::vec4(0.0f);
					vert.joint0 = hasSkin ? glm::vec4(glm::make_vec4(&bufferJoints[v * 4])) : glm::vec4(0.0f);
					vert.weight0 = hasSkin ? glm::make_vec4(&bufferWeights[v * 4]) : glm::vec4(0.0f);
					vertexBuffer.push_back(vert);
				}
			}
			// Indices
			{
				const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
				const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

				indexCount = static_cast<std::uint32_t>(accessor.count);

				switch (accessor.componentType)
				{
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
					{
						std::uint32_t* buf = new std::uint32_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(std::uint32_t));
						for (size_t index = 0; index < accessor.count; index++)
						{
							indexBuffer.push_back(buf[index] + vertexStart);
						}
						delete[] buf;
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
					{
						uint16_t* buf = new uint16_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
						for (size_t index = 0; index < accessor.count; index++)
						{
							indexBuffer.push_back(buf[index] + vertexStart);
						}
						delete[] buf;
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
					{
						uint8_t* buf = new uint8_t[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
						for (size_t index = 0; index < accessor.count; index++)
						{
							indexBuffer.push_back(buf[index] + vertexStart);
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
			newPrimitive->setDimensions(posMin, posMax);
			newMesh->primitives.push_back(newPrimitive);
		}
		newNode->mesh = newMesh;
	}
	if (parent)
	{
		parent->children.push_back(newNode);
	}
	else
	{
		nodes.push_back(newNode);
	}
	linearNodes.push_back(newNode);
}

void vkglTF::Model::loadSkins(tinygltf::Model& gltfModel)
{
	for (tinygltf::Skin& source : gltfModel.skins)
	{
		Skin* newSkin = new Skin{};
		newSkin->name = source.name;

		// Find skeleton root node
		if (source.skeleton > -1)
		{
			newSkin->skeletonRoot = NodeFromIndex(source.skeleton);
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
			const tinygltf::Accessor& accessor = gltfModel.accessors[source.inverseBindMatrices];
			const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
			const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];
			newSkin->inverseBindMatrices.resize(accessor.count);
			memcpy(newSkin->inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
		}

		skins.push_back(newSkin);
	}
}

void vkglTF::Model::LoadImages(tinygltf::Model& aGlTFModel, VulkanDevice* aDevice, VkQueue aTransferQueue)
{
	for (tinygltf::Image& image : aGlTFModel.images)
	{
		vkglTF::Texture texture;
		texture.FromGlTfImage(image, path, aDevice, aTransferQueue);
		texture.mIndex = static_cast<std::uint32_t>(textures.size());
		textures.push_back(texture);
	}
	// Create an empty texture to be used for empty material images
	CreateEmptyTexture(aTransferQueue);
}

void vkglTF::Model::loadMaterials(tinygltf::Model& gltfModel)
{
	for (tinygltf::Material& mat : gltfModel.materials)
	{
		vkglTF::Material material(mVulkanDevice);
		if (mat.values.find("baseColorTexture") != mat.values.end())
		{
			material.baseColorTexture = GetTexture(gltfModel.textures[mat.values["baseColorTexture"].TextureIndex()].source);
		}
		// Metallic roughness workflow
		if (mat.values.find("metallicRoughnessTexture") != mat.values.end())
		{
			material.metallicRoughnessTexture = GetTexture(gltfModel.textures[mat.values["metallicRoughnessTexture"].TextureIndex()].source);
		}
		if (mat.values.find("roughnessFactor") != mat.values.end())
		{
			material.roughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
		}
		if (mat.values.find("metallicFactor") != mat.values.end())
		{
			material.metallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
		}
		if (mat.values.find("baseColorFactor") != mat.values.end())
		{
			material.baseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
		}
		if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end())
		{
			material.normalTexture = GetTexture(gltfModel.textures[mat.additionalValues["normalTexture"].TextureIndex()].source);
		}
		else
		{
			material.normalTexture = &mEmptyTexture;
		}
		if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end())
		{
			material.emissiveTexture = GetTexture(gltfModel.textures[mat.additionalValues["emissiveTexture"].TextureIndex()].source);
		}
		if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end())
		{
			material.occlusionTexture = GetTexture(gltfModel.textures[mat.additionalValues["occlusionTexture"].TextureIndex()].source);
		}
		if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end())
		{
			const tinygltf::Parameter param = mat.additionalValues["alphaMode"];
			if (param.string_value == "BLEND")
			{
				material.alphaMode = Material::AlphaMode::ALPHAMODE_BLEND;
			}
			if (param.string_value == "MASK")
			{
				material.alphaMode = Material::AlphaMode::ALPHAMODE_MASK;
			}
		}
		if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end())
		{
			material.alphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
		}

		materials.push_back(material);
	}
	// Push a default material at the end of the list for meshes with no material assigned
	materials.push_back(Material(mVulkanDevice));
}

void vkglTF::Model::loadAnimations(tinygltf::Model& gltfModel)
{
	for (tinygltf::Animation& anim : gltfModel.animations)
	{
		vkglTF::Animation animation{};
		animation.name = anim.name;
		if (anim.name.empty())
		{
			animation.name = std::to_string(animations.size());
		}

		// Samplers
		for (auto& samp : anim.samplers)
		{
			vkglTF::AnimationSampler sampler{};

			if (samp.interpolation == "LINEAR")
			{
				sampler.interpolation = AnimationSampler::InterpolationType::LINEAR;
			}
			if (samp.interpolation == "STEP")
			{
				sampler.interpolation = AnimationSampler::InterpolationType::STEP;
			}
			if (samp.interpolation == "CUBICSPLINE")
			{
				sampler.interpolation = AnimationSampler::InterpolationType::CUBICSPLINE;
			}

			// Read sampler input time values
			{
				const tinygltf::Accessor& accessor = gltfModel.accessors[samp.input];
				const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				float* buf = new float[accessor.count];
				memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(float));
				for (size_t index = 0; index < accessor.count; index++)
				{
					sampler.inputs.push_back(buf[index]);
				}
				delete[] buf;
				for (auto input : sampler.inputs)
				{
					if (input < animation.start)
					{
						animation.start = input;
					};
					if (input > animation.end)
					{
						animation.end = input;
					}
				}
			}

			// Read sampler output T/R/S values 
			{
				const tinygltf::Accessor& accessor = gltfModel.accessors[samp.output];
				const tinygltf::BufferView& bufferView = gltfModel.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = gltfModel.buffers[bufferView.buffer];

				assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

				switch (accessor.type)
				{
					case TINYGLTF_TYPE_VEC3:
					{
						glm::vec3* buf = new glm::vec3[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::vec3));
						for (size_t index = 0; index < accessor.count; index++)
						{
							sampler.outputsVec4.push_back(glm::vec4(buf[index], 0.0f));
						}
						delete[] buf;
						break;
					}
					case TINYGLTF_TYPE_VEC4:
					{
						glm::vec4* buf = new glm::vec4[accessor.count];
						memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::vec4));
						for (size_t index = 0; index < accessor.count; index++)
						{
							sampler.outputsVec4.push_back(buf[index]);
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

			animation.samplers.push_back(sampler);
		}

		// Channels
		for (auto& source : anim.channels)
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
			channel.samplerIndex = source.sampler;
			channel.node = NodeFromIndex(source.target_node);
			if (!channel.node)
			{
				continue;
			}

			animation.channels.push_back(channel);
		}

		animations.push_back(animation);
	}
}

void vkglTF::Model::LoadFromFile(const std::filesystem::path& aPath, VulkanDevice* aDevice, VkQueue aTransferQueue, std::uint32_t aFileLoadingFlags, float aScale)
{
	tinygltf::Model gltfModel;
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

	std::string error, warning;

	mVulkanDevice = aDevice;

	bool fileLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, pathString);

	std::vector<std::uint32_t> indexBuffer;
	std::vector<Vertex> vertexBuffer;

	if (fileLoaded)
	{
		if (!(aFileLoadingFlags & FileLoadingFlags::DontLoadImages))
		{
			LoadImages(gltfModel, aDevice, aTransferQueue);
		}
		loadMaterials(gltfModel);
		const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];
		for (size_t i = 0; i < scene.nodes.size(); i++)
		{
			const tinygltf::Node node = gltfModel.nodes[scene.nodes[i]];
			loadNode(nullptr, node, scene.nodes[i], gltfModel, indexBuffer, vertexBuffer, aScale);
		}
		if (gltfModel.animations.size() > 0)
		{
			loadAnimations(gltfModel);
		}
		loadSkins(gltfModel);

		for (auto node : linearNodes)
		{
			// Assign skins
			if (node->skinIndex > -1)
			{
				node->skin = skins[node->skinIndex];
			}
			// Initial pose
			if (node->mesh)
			{
				node->update();
			}
		}
	}
	else
	{
		throw std::runtime_error(std::format("Could not load glTF file: {} {}", aPath.generic_string(), error));
	}

	// Pre-Calculations for requested features
	if ((aFileLoadingFlags & FileLoadingFlags::PreTransformVertices) || (aFileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors) || (aFileLoadingFlags & FileLoadingFlags::FlipY))
	{
		const bool preTransform = aFileLoadingFlags & FileLoadingFlags::PreTransformVertices;
		const bool preMultiplyColor = aFileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors;
		const bool flipY = aFileLoadingFlags & FileLoadingFlags::FlipY;
		for (Node* node : linearNodes)
		{
			if (node->mesh)
			{
				const glm::mat4 localMatrix = node->GetMatrix();
				for (Primitive* primitive : node->mesh->primitives)
				{
					for (std::uint32_t i = 0; i < primitive->vertexCount; i++)
					{
						Vertex& vertex = vertexBuffer[primitive->firstVertex + i];
						// Pre-transform vertex positions by node-hierarchy
						if (preTransform)
						{
							vertex.pos = glm::vec3(localMatrix * glm::vec4(vertex.pos, 1.0f));
							vertex.normal = glm::normalize(glm::mat3(localMatrix) * vertex.normal);
						}
						// Flip Y-Axis of vertex positions
						if (flipY)
						{
							vertex.pos.y *= -1.0f;
							vertex.normal.y *= -1.0f;
						}
						// Pre-Multiply vertex colors with material base color
						if (preMultiplyColor)
						{
							vertex.color = primitive->material.baseColorFactor * vertex.color;
						}
					}
				}
			}
		}
	}

	for (auto& extension : gltfModel.extensionsUsed)
	{
		if (extension == "KHR_materials_pbrSpecularGlossiness")
		{
			std::cout << "Required extension: " << extension;
			metallicRoughnessWorkflow = false;
		}
	}

	size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);
	size_t indexBufferSize = indexBuffer.size() * sizeof(std::uint32_t);
	indices.count = static_cast<std::uint32_t>(indexBuffer.size());
	vertices.count = static_cast<std::uint32_t>(vertexBuffer.size());

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
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | memoryPropertyFlags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBufferSize,
		&vertices.buffer,
		&vertices.memory, nullptr));
	// Index buffer
	VK_CHECK_RESULT(aDevice->CreateBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | memoryPropertyFlags,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBufferSize,
		&indices.buffer,
		&indices.memory, nullptr));

	// Copy from staging buffers
	VkCommandBuffer copyCmd = aDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertices.buffer, 1, &copyRegion);

	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indices.buffer, 1, &copyRegion);

	aDevice->flushCommandBuffer(copyCmd, aTransferQueue, true);

	vkDestroyBuffer(aDevice->mLogicalVkDevice, vertexStaging.buffer, nullptr);
	vkFreeMemory(aDevice->mLogicalVkDevice, vertexStaging.memory, nullptr);
	vkDestroyBuffer(aDevice->mLogicalVkDevice, indexStaging.buffer, nullptr);
	vkFreeMemory(aDevice->mLogicalVkDevice, indexStaging.memory, nullptr);

	GetSceneDimensions();

	// Setup descriptors
	std::uint32_t uboCount{0};
	std::uint32_t imageCount{0};
	for (auto& node : linearNodes)
	{
		if (node->mesh)
		{
			uboCount++;
		}
	}
	for (auto& material : materials)
	{
		if (material.baseColorTexture != nullptr)
		{
			imageCount++;
		}
	}
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboCount },
	};
	if (imageCount > 0)
	{
		if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
		{
			poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount});
		}
		if (descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap)
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
		if (descriptorSetLayoutUbo == VK_NULL_HANDLE)
		{
			VkDescriptorSetLayoutBinding setLayoutBinding{.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT};
			VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &setLayoutBinding};
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(aDevice->mLogicalVkDevice, &descriptorLayoutCI, nullptr, &descriptorSetLayoutUbo));
		}
		for (auto node : nodes)
		{
			PrepareNodeDescriptor(node, descriptorSetLayoutUbo);
		}
	}

	// Descriptors for per-material images
	{
		// Layout is global, so only create if it hasn't already been created before
		if (descriptorSetLayoutImage == VK_NULL_HANDLE)
		{
			std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
			if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
			{
				setLayoutBindings.push_back({.binding = static_cast<std::uint32_t>(setLayoutBindings.size()), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT});
			}
			if (descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap)
			{
				setLayoutBindings.push_back({.binding = static_cast<std::uint32_t>(setLayoutBindings.size()), .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT});
			}
			VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.bindingCount = static_cast<std::uint32_t>(setLayoutBindings.size()),
				.pBindings = setLayoutBindings.data(),
			};
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(aDevice->mLogicalVkDevice, &descriptorLayoutCI, nullptr, &descriptorSetLayoutImage));
		}
		for (auto& material : materials)
		{
			if (material.baseColorTexture != nullptr)
			{
				material.CreateDescriptorSet(descriptorPool, vkglTF::descriptorSetLayoutImage, descriptorBindingFlags);
			}
		}
	}
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

	size_t bufferSize = mEmptyTexture.mWidth * mEmptyTexture.mHeight * 4;
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
	memcpy(data, buffer, bufferSize);
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
	VulkanTools::setImageLayout(copyCmd, mEmptyTexture.mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
	vkCmdCopyBufferToImage(copyCmd, stagingBuffer, mEmptyTexture.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
	VulkanTools::setImageLayout(copyCmd, mEmptyTexture.mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
	mVulkanDevice->flushCommandBuffer(copyCmd, aTransferQueue, true);
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
	vkCmdBindVertexBuffers(aCommandBuffer, 0, 1, &vertices.buffer, offsets);
	vkCmdBindIndexBuffer(aCommandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	buffersBound = true;
}

void vkglTF::Model::DrawNode(Node* aNode, VkCommandBuffer aCommandBuffer, std::uint32_t aRenderFlags, VkPipelineLayout aPipelineLayout, std::uint32_t aBindImageSet)
{
	if (aNode->mesh)
	{
		for (Primitive* primitive : aNode->mesh->primitives)
		{
			bool skip = false;
			const vkglTF::Material& material = primitive->material;
			if (aRenderFlags & RenderFlags::RenderOpaqueNodes)
			{
				skip = (material.alphaMode != Material::AlphaMode::ALPHAMODE_OPAQUE);
			}
			if (aRenderFlags & RenderFlags::RenderAlphaMaskedNodes)
			{
				skip = (material.alphaMode != Material::AlphaMode::ALPHAMODE_MASK);
			}
			if (aRenderFlags & RenderFlags::RenderAlphaBlendedNodes)
			{
				skip = (material.alphaMode != Material::AlphaMode::ALPHAMODE_BLEND);
			}
			if (!skip)
			{
				if (aRenderFlags & RenderFlags::BindImages)
				{
					vkCmdBindDescriptorSets(aCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aPipelineLayout, aBindImageSet, 1, &material.descriptorSet, 0, nullptr);
				}
				vkCmdDrawIndexed(aCommandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
			}
		}
	}
	for (auto& child : aNode->children)
	{
		DrawNode(child, aCommandBuffer, aRenderFlags, aPipelineLayout, aBindImageSet);
	}
}

void vkglTF::Model::Draw(VkCommandBuffer aCommandBuffer, std::uint32_t aRenderFlags, VkPipelineLayout aPipelineLayout, std::uint32_t aBindImageSet)
{
	if (!buffersBound)
	{
		const VkDeviceSize offsets[1] = {0};
		vkCmdBindVertexBuffers(aCommandBuffer, 0, 1, &vertices.buffer, offsets);
		vkCmdBindIndexBuffer(aCommandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	}
	for (auto& node : nodes)
	{
		DrawNode(node, aCommandBuffer, aRenderFlags, aPipelineLayout, aBindImageSet);
	}
}

void vkglTF::Model::GetNodeDimensions(Node* aNode, glm::vec3& aMin, glm::vec3& aMax)
{
	if (aNode->mesh)
	{
		for (Primitive* primitive : aNode->mesh->primitives)
		{
			glm::vec4 locMin = glm::vec4(primitive->dimensions.min, 1.0f) * aNode->GetMatrix();
			glm::vec4 locMax = glm::vec4(primitive->dimensions.max, 1.0f) * aNode->GetMatrix();
			if (locMin.x < aMin.x) { aMin.x = locMin.x; }
			if (locMin.y < aMin.y) { aMin.y = locMin.y; }
			if (locMin.z < aMin.z) { aMin.z = locMin.z; }
			if (locMax.x > aMax.x) { aMax.x = locMax.x; }
			if (locMax.y > aMax.y) { aMax.y = locMax.y; }
			if (locMax.z > aMax.z) { aMax.z = locMax.z; }
		}
	}
	for (auto child : aNode->children)
	{
		GetNodeDimensions(child, aMin, aMax);
	}
}

void vkglTF::Model::GetSceneDimensions()
{
	dimensions.min = glm::vec3(FLT_MAX);
	dimensions.max = glm::vec3(-FLT_MAX);
	for (auto node : nodes)
	{
		GetNodeDimensions(node, dimensions.min, dimensions.max);
	}
	dimensions.size = dimensions.max - dimensions.min;
	dimensions.center = (dimensions.min + dimensions.max) / 2.0f;
	dimensions.radius = glm::distance(dimensions.min, dimensions.max) / 2.0f;
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
	for (auto& channel : animation.channels)
	{
		vkglTF::AnimationSampler& sampler = animation.samplers[channel.samplerIndex];
		if (sampler.inputs.size() > sampler.outputsVec4.size())
		{
			continue;
		}

		for (auto i = 0; i < sampler.inputs.size() - 1; i++)
		{
			if ((aTime >= sampler.inputs[i]) && (aTime <= sampler.inputs[i + 1]))
			{
				float u = std::max(0.0f, aTime - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
				if (u <= 1.0f)
				{
					switch (channel.mPathType)
					{
						case vkglTF::AnimationChannel::PathType::TRANSLATION:
						{
							glm::vec4 trans = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], u);
							channel.node->translation = glm::vec3(trans);
							break;
						}
						case vkglTF::AnimationChannel::PathType::SCALE:
						{
							glm::vec4 trans = glm::mix(sampler.outputsVec4[i], sampler.outputsVec4[i + 1], u);
							channel.node->scale = glm::vec3(trans);
							break;
						}
						case vkglTF::AnimationChannel::PathType::ROTATION:
						{
							glm::quat q1;
							q1.x = sampler.outputsVec4[i].x;
							q1.y = sampler.outputsVec4[i].y;
							q1.z = sampler.outputsVec4[i].z;
							q1.w = sampler.outputsVec4[i].w;
							glm::quat q2;
							q2.x = sampler.outputsVec4[i + 1].x;
							q2.y = sampler.outputsVec4[i + 1].y;
							q2.z = sampler.outputsVec4[i + 1].z;
							q2.w = sampler.outputsVec4[i + 1].w;
							channel.node->rotation = glm::normalize(glm::slerp(q1, q2, u));
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
		for (auto& node : nodes)
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
	if (aParent->index == aIndex)
	{
		return aParent;
	}
	for (auto& child : aParent->children)
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
	for (auto& node : nodes)
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
	if (aNode->mesh)
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &aDescriptorSetLayout
		};
		VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &descriptorSetAllocInfo, &aNode->mesh->uniformBuffer.descriptorSet));
		VkWriteDescriptorSet writeDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = aNode->mesh->uniformBuffer.descriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &aNode->mesh->uniformBuffer.descriptor
		};
		vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, 1, &writeDescriptorSet, 0, nullptr);
	}

	for (auto& child : aNode->children)
	{
		PrepareNodeDescriptor(child, aDescriptorSetLayout);
	}
}
