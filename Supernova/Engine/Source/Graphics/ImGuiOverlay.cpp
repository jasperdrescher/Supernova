#include "ImGuiOverlay.hpp"

#include "FileLoader.hpp"
#include "Math/Functions.hpp"
#include "Math/Types.hpp"
#include "VulkanInitializers.hpp"
#include "VulkanTools.hpp"
#include "VulkanTypes.hpp"

#define GLFW_EXCLUDE_API
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>

#include <imgui.h>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace ImGuiOverlayPrivate
{
	static ImGuiKey KeyToImGuiKey(int aKeycode, int /*aScancode*/)
	{
		switch (aKeycode)
		{
			case GLFW_KEY_TAB: return ImGuiKey_Tab;
			case GLFW_KEY_LEFT: return ImGuiKey_LeftArrow;
			case GLFW_KEY_RIGHT: return ImGuiKey_RightArrow;
			case GLFW_KEY_UP: return ImGuiKey_UpArrow;
			case GLFW_KEY_DOWN: return ImGuiKey_DownArrow;
			case GLFW_KEY_PAGE_UP: return ImGuiKey_PageUp;
			case GLFW_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
			case GLFW_KEY_HOME: return ImGuiKey_Home;
			case GLFW_KEY_END: return ImGuiKey_End;
			case GLFW_KEY_INSERT: return ImGuiKey_Insert;
			case GLFW_KEY_DELETE: return ImGuiKey_Delete;
			case GLFW_KEY_BACKSPACE: return ImGuiKey_Backspace;
			case GLFW_KEY_SPACE: return ImGuiKey_Space;
			case GLFW_KEY_ENTER: return ImGuiKey_Enter;
			case GLFW_KEY_ESCAPE: return ImGuiKey_Escape;
			case GLFW_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
			case GLFW_KEY_COMMA: return ImGuiKey_Comma;
			case GLFW_KEY_MINUS: return ImGuiKey_Minus;
			case GLFW_KEY_PERIOD: return ImGuiKey_Period;
			case GLFW_KEY_SLASH: return ImGuiKey_Slash;
			case GLFW_KEY_SEMICOLON: return ImGuiKey_Semicolon;
			case GLFW_KEY_EQUAL: return ImGuiKey_Equal;
			case GLFW_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
			case GLFW_KEY_BACKSLASH: return ImGuiKey_Backslash;
			case GLFW_KEY_WORLD_1: return ImGuiKey_Oem102;
			case GLFW_KEY_WORLD_2: return ImGuiKey_Oem102;
			case GLFW_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
			case GLFW_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
			case GLFW_KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
			case GLFW_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
			case GLFW_KEY_NUM_LOCK: return ImGuiKey_NumLock;
			case GLFW_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
			case GLFW_KEY_PAUSE: return ImGuiKey_Pause;
			case GLFW_KEY_KP_0: return ImGuiKey_Keypad0;
			case GLFW_KEY_KP_1: return ImGuiKey_Keypad1;
			case GLFW_KEY_KP_2: return ImGuiKey_Keypad2;
			case GLFW_KEY_KP_3: return ImGuiKey_Keypad3;
			case GLFW_KEY_KP_4: return ImGuiKey_Keypad4;
			case GLFW_KEY_KP_5: return ImGuiKey_Keypad5;
			case GLFW_KEY_KP_6: return ImGuiKey_Keypad6;
			case GLFW_KEY_KP_7: return ImGuiKey_Keypad7;
			case GLFW_KEY_KP_8: return ImGuiKey_Keypad8;
			case GLFW_KEY_KP_9: return ImGuiKey_Keypad9;
			case GLFW_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
			case GLFW_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
			case GLFW_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
			case GLFW_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
			case GLFW_KEY_KP_ADD: return ImGuiKey_KeypadAdd;
			case GLFW_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
			case GLFW_KEY_KP_EQUAL: return ImGuiKey_KeypadEqual;
			case GLFW_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
			case GLFW_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
			case GLFW_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
			case GLFW_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
			case GLFW_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
			case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
			case GLFW_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
			case GLFW_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
			case GLFW_KEY_MENU: return ImGuiKey_Menu;
			case GLFW_KEY_0: return ImGuiKey_0;
			case GLFW_KEY_1: return ImGuiKey_1;
			case GLFW_KEY_2: return ImGuiKey_2;
			case GLFW_KEY_3: return ImGuiKey_3;
			case GLFW_KEY_4: return ImGuiKey_4;
			case GLFW_KEY_5: return ImGuiKey_5;
			case GLFW_KEY_6: return ImGuiKey_6;
			case GLFW_KEY_7: return ImGuiKey_7;
			case GLFW_KEY_8: return ImGuiKey_8;
			case GLFW_KEY_9: return ImGuiKey_9;
			case GLFW_KEY_A: return ImGuiKey_A;
			case GLFW_KEY_B: return ImGuiKey_B;
			case GLFW_KEY_C: return ImGuiKey_C;
			case GLFW_KEY_D: return ImGuiKey_D;
			case GLFW_KEY_E: return ImGuiKey_E;
			case GLFW_KEY_F: return ImGuiKey_F;
			case GLFW_KEY_G: return ImGuiKey_G;
			case GLFW_KEY_H: return ImGuiKey_H;
			case GLFW_KEY_I: return ImGuiKey_I;
			case GLFW_KEY_J: return ImGuiKey_J;
			case GLFW_KEY_K: return ImGuiKey_K;
			case GLFW_KEY_L: return ImGuiKey_L;
			case GLFW_KEY_M: return ImGuiKey_M;
			case GLFW_KEY_N: return ImGuiKey_N;
			case GLFW_KEY_O: return ImGuiKey_O;
			case GLFW_KEY_P: return ImGuiKey_P;
			case GLFW_KEY_Q: return ImGuiKey_Q;
			case GLFW_KEY_R: return ImGuiKey_R;
			case GLFW_KEY_S: return ImGuiKey_S;
			case GLFW_KEY_T: return ImGuiKey_T;
			case GLFW_KEY_U: return ImGuiKey_U;
			case GLFW_KEY_V: return ImGuiKey_V;
			case GLFW_KEY_W: return ImGuiKey_W;
			case GLFW_KEY_X: return ImGuiKey_X;
			case GLFW_KEY_Y: return ImGuiKey_Y;
			case GLFW_KEY_Z: return ImGuiKey_Z;
			case GLFW_KEY_F1: return ImGuiKey_F1;
			case GLFW_KEY_F2: return ImGuiKey_F2;
			case GLFW_KEY_F3: return ImGuiKey_F3;
			case GLFW_KEY_F4: return ImGuiKey_F4;
			case GLFW_KEY_F5: return ImGuiKey_F5;
			case GLFW_KEY_F6: return ImGuiKey_F6;
			case GLFW_KEY_F7: return ImGuiKey_F7;
			case GLFW_KEY_F8: return ImGuiKey_F8;
			case GLFW_KEY_F9: return ImGuiKey_F9;
			case GLFW_KEY_F10: return ImGuiKey_F10;
			case GLFW_KEY_F11: return ImGuiKey_F11;
			case GLFW_KEY_F12: return ImGuiKey_F12;
			case GLFW_KEY_F13: return ImGuiKey_F13;
			case GLFW_KEY_F14: return ImGuiKey_F14;
			case GLFW_KEY_F15: return ImGuiKey_F15;
			case GLFW_KEY_F16: return ImGuiKey_F16;
			case GLFW_KEY_F17: return ImGuiKey_F17;
			case GLFW_KEY_F18: return ImGuiKey_F18;
			case GLFW_KEY_F19: return ImGuiKey_F19;
			case GLFW_KEY_F20: return ImGuiKey_F20;
			case GLFW_KEY_F21: return ImGuiKey_F21;
			case GLFW_KEY_F22: return ImGuiKey_F22;
			case GLFW_KEY_F23: return ImGuiKey_F23;
			case GLFW_KEY_F24: return ImGuiKey_F24;
			default: return ImGuiKey_None;
		}
	}

	static int TranslateUntranslatedKey(int aKey, int aScancode)
	{
		// GLFW 3.1+ attempts to "untranslate" keys, which goes the opposite of what every other framework does, making using lettered shortcuts difficult.
		// (It had reasons to do so: namely GLFW is/was more likely to be used for WASD-type game controls rather than lettered shortcuts, but IHMO the 3.1 change could have been done differently)
		// See https://github.com/glfw/glfw/issues/1502 for details.
		// Adding a workaround to undo this (so our keys are translated->untranslated->translated, likely a lossy process).
		// This won't cover edge cases but this is at least going to cover common cases.
		if (aKey >= GLFW_KEY_KP_0 && aKey <= GLFW_KEY_KP_EQUAL)
			return aKey;

		const char* key_name = glfwGetKeyName(aKey, aScancode);
		if (key_name && key_name[0] != 0 && key_name[1] == 0)
		{
			const char char_names[] = "`-=[]\\,;\'./";
			const int char_keys[] = {GLFW_KEY_GRAVE_ACCENT, GLFW_KEY_MINUS, GLFW_KEY_EQUAL, GLFW_KEY_LEFT_BRACKET, GLFW_KEY_RIGHT_BRACKET, GLFW_KEY_BACKSLASH, GLFW_KEY_COMMA, GLFW_KEY_SEMICOLON, GLFW_KEY_APOSTROPHE, GLFW_KEY_PERIOD, GLFW_KEY_SLASH, 0};
			IM_ASSERT(IM_ARRAYSIZE(char_names) == IM_ARRAYSIZE(char_keys));
			if (key_name[0] >= '0' && key_name[0] <= '9') { aKey = GLFW_KEY_0 + (key_name[0] - '0'); }
			else if (key_name[0] >= 'A' && key_name[0] <= 'Z') { aKey = GLFW_KEY_A + (key_name[0] - 'A'); }
			else if (key_name[0] >= 'a' && key_name[0] <= 'z') { aKey = GLFW_KEY_A + (key_name[0] - 'a'); }
			else if (const char* p = strchr(char_names, key_name[0])) { aKey = char_keys[p - char_names]; }
		}
		// if (action == GLFW_PRESS) printf("key %d scancode %d name '%s'\n", key, scancode, key_name);
		return aKey;
	}
}

ImGuiOverlay::ImGuiOverlay()
	: mVulkanDevice{nullptr}
	, mQueue{VK_NULL_HANDLE}
	, mRasterizationSamples{VK_SAMPLE_COUNT_1_BIT}
	, mSubpass{0}
	, gMaxConcurrentFrames{0}
	, mCurrentBufferIndex{0}
	, mDescriptorPool{VK_NULL_HANDLE}
	, mDescriptorSetLayout{VK_NULL_HANDLE}
	, mDescriptorSet{VK_NULL_HANDLE}
	, mPipelineLayout{VK_NULL_HANDLE}
	, mPipeline{VK_NULL_HANDLE}
	, mFontMemory{VK_NULL_HANDLE}
	, mFontImage{VK_NULL_HANDLE}
	, mFontImageView{VK_NULL_HANDLE}
	, mSampler{VK_NULL_HANDLE}
	, mIsVisible{true}
	, mScale{1.0f}
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
}

ImGuiOverlay::~ImGuiOverlay()
{
	if (ImGui::GetCurrentContext())
	{
		ImGui::DestroyContext();
	}
}

/** Prepare all vulkan resources required to render the UI overlay */
void ImGuiOverlay::PrepareResources()
{
	assert(gMaxConcurrentFrames > 0);

	ImGuiIO& io = ImGui::GetIO();

	unsigned char* fontData = nullptr;
	int texWidth = 0;
	int texHeight = 0;
	const std::filesystem::path fontPath = "Roboto-Medium.ttf";
	const std::filesystem::path filePath = FileLoader::GetEngineResourcesPath() / FileLoader::gFontPath / fontPath;
	io.Fonts->AddFontFromFileTTF(filePath.generic_string().c_str(), 16.0f * mScale);
	io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
	const VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

	InitializeStyle();

	const VkImageCreateInfo imageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.extent = {.width = (std::uint32_t)texWidth, .height = (std::uint32_t)texHeight, .depth = 1 },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalVkDevice, &imageCreateInfo, nullptr, &mFontImage));

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalVkDevice, mFontImage, &memoryRequirements);

	const VkMemoryAllocateInfo memoryAllocateInfo{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = mVulkanDevice->GetMemoryTypeIndex(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	};
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalVkDevice, &memoryAllocateInfo, nullptr, &mFontMemory));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalVkDevice, mFontImage, mFontMemory, 0));

	const VkImageViewCreateInfo imageViewCreateInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = mFontImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
	};
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalVkDevice, &imageViewCreateInfo, nullptr, &mFontImageView));

	Buffer stagingBuffer;
	VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, uploadSize));
	VK_CHECK_RESULT(stagingBuffer.Map());
	std::memcpy(stagingBuffer.mMappedData, fontData, uploadSize);

	VkCommandBuffer copyCommandBuffer = mVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VulkanTools::SetImageLayout(
		copyCommandBuffer,
		mFontImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT);
	
	const VkBufferImageCopy bufferCopyRegion{
		.imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1 },
		.imageExtent = {.width = (std::uint32_t)texWidth, .height = (std::uint32_t)texHeight, .depth = 1 }
	};
	vkCmdCopyBufferToImage(copyCommandBuffer, stagingBuffer.mVkBuffer, mFontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

	VulkanTools::SetImageLayout(
		copyCommandBuffer,
		mFontImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	mVulkanDevice->FlushCommandBuffer(copyCommandBuffer, mQueue, true);

	stagingBuffer.Destroy();

	const VkSamplerCreateInfo samplerCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.maxAnisotropy = 1.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	};
	VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->mLogicalVkDevice, &samplerCreateInfo, nullptr, &mSampler));

	const VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1};
	const VkDescriptorPoolCreateInfo descriptorPoolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 2, .poolSizeCount = 1, .pPoolSizes = &poolSize};
	VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalVkDevice, &descriptorPoolInfo, nullptr, &mDescriptorPool));

	const VkDescriptorSetLayoutBinding setLayoutBinding = VulkanInitializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	const VkDescriptorSetLayoutCreateInfo descriptorLayout{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &setLayoutBinding};
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, &descriptorLayout, nullptr, &mDescriptorSetLayout));

	const VkDescriptorSetAllocateInfo allocInfo = VulkanInitializers::DescriptorSetAllocateInfo(mDescriptorPool, &mDescriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalVkDevice, &allocInfo, &mDescriptorSet));

	const VkDescriptorImageInfo fontDescriptor{.sampler = mSampler, .imageView = mFontImageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	const VkWriteDescriptorSet writeDescriptorSets = VulkanInitializers::WriteDescriptorSet(mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor);
	vkUpdateDescriptorSets(mVulkanDevice->mLogicalVkDevice, 1, &writeDescriptorSets, 0, nullptr);

	// Buffers per max. frames-in-flight
	mBuffers.resize(gMaxConcurrentFrames);
}

void ImGuiOverlay::InitializeStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
	style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
	
	style.ScaleAllSizes(mScale);
	style.FontScaleDpi = mScale;
}

/** Prepare a separate pipeline for the UI overlay rendering decoupled from the main application */
void ImGuiOverlay::PreparePipeline(const VkPipelineCache aPipelineCache, const VkFormat aColorFormat, const VkFormat aDepthFormat)
{
	// Pipeline layout
	// Push constants for UI rendering parameters
	const VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(PushConstBlock)};
	const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &mDescriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalVkDevice, &pipelineLayoutCreateInfo, nullptr, &mPipelineLayout));

	// Setup graphics pipeline for UI rendering
	const VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = VulkanInitializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	const VkPipelineRasterizationStateCreateInfo rasterizationState = VulkanInitializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

	// Enable blending
	const VkPipelineColorBlendAttachmentState blendAttachmentState{
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};

	// Vertex bindings an attributes based on ImGui vertex definition
	const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		{.binding = 0, .stride = sizeof(ImDrawVert), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX }
	};

	const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		{.location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(ImDrawVert, pos) },
		{.location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(ImDrawVert, uv) },
		{.location = 2, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UNORM, .offset = offsetof(ImDrawVert, col) },
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = VulkanInitializers::PipelineVertexInputStateCreateInfo();
	vertexInputState.vertexBindingDescriptionCount = static_cast<std::uint32_t>(vertexInputBindings.size());
	vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputState.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertexInputAttributes.size());
	vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

	const VkPipelineColorBlendStateCreateInfo colorBlendState = VulkanInitializers::PipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	const VkPipelineDepthStencilStateCreateInfo depthStencilState = VulkanInitializers::PipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
	const VkPipelineViewportStateCreateInfo viewportState = VulkanInitializers::PipelineViewportStateCreateInfo(1, 1, 0);
	const VkPipelineMultisampleStateCreateInfo multisampleState = VulkanInitializers::PipelineMultisampleStateCreateInfo(mRasterizationSamples);
	const std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	const VkPipelineDynamicStateCreateInfo dynamicState = VulkanInitializers::PipelineDynamicStateCreateInfo(dynamicStateEnables);
	VkGraphicsPipelineCreateInfo pipelineCreateInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = static_cast<std::uint32_t>(mShaders.size()),
		.pStages = mShaders.data(),
		.pVertexInputState = &vertexInputState,
		.pInputAssemblyState = &inputAssemblyState,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizationState,
		.pMultisampleState = &multisampleState,
		.pDepthStencilState = &depthStencilState,
		.pColorBlendState = &colorBlendState,
		.pDynamicState = &dynamicState,
		.layout = mPipelineLayout,
		.subpass = mSubpass,
	};

	// If we are using dynamic rendering (renderPass is null), we must define color, depth and stencil attachments at pipeline create time
	const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &aColorFormat,
			.depthAttachmentFormat = aDepthFormat,
			.stencilAttachmentFormat = aDepthFormat
	};
	pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalVkDevice, aPipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipeline));
}

/** Update vertex and index buffer containing the imGui elements when required */
void ImGuiOverlay::Update(std::uint32_t aCurrentBufferIndex)
{
	ImDrawData* imDrawData = ImGui::GetDrawData();

	if (!imDrawData)
	{
		return;
	}

	// Note: Alignment is done inside buffer creation
	VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
	VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

	// Update buffers only if vertex or index count has been changed compared to current buffer size
	if ((vertexBufferSize == 0) || (indexBufferSize == 0))
	{
		return;
	}

	// Create buffers with multiple of a chunk size to minimize the need to recreate them
	const VkDeviceSize chunkSize = 16384;
	vertexBufferSize = ((vertexBufferSize + chunkSize - 1) / chunkSize) * chunkSize;
	indexBufferSize = ((indexBufferSize + chunkSize - 1) / chunkSize) * chunkSize;

	// Recreate vertex buffer only if necessary
	if ((mBuffers[aCurrentBufferIndex].vertexBuffer.mVkBuffer == VK_NULL_HANDLE) || (mBuffers[aCurrentBufferIndex].vertexBuffer.mVkDeviceSize < vertexBufferSize))
	{
		mBuffers[aCurrentBufferIndex].vertexBuffer.Unmap();
		mBuffers[aCurrentBufferIndex].vertexBuffer.Destroy();
		VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mBuffers[aCurrentBufferIndex].vertexBuffer, vertexBufferSize));
		mBuffers[aCurrentBufferIndex].vertexCount = imDrawData->TotalVtxCount;
		mBuffers[aCurrentBufferIndex].vertexBuffer.Map();
	}

	// Recreate index buffer only if necessary
	if ((mBuffers[aCurrentBufferIndex].indexBuffer.mVkBuffer == VK_NULL_HANDLE) || (mBuffers[aCurrentBufferIndex].indexBuffer.mVkDeviceSize < indexBufferSize))
	{
		mBuffers[aCurrentBufferIndex].indexBuffer.Unmap();
		mBuffers[aCurrentBufferIndex].indexBuffer.Destroy();
		VK_CHECK_RESULT(mVulkanDevice->CreateBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mBuffers[aCurrentBufferIndex].indexBuffer, indexBufferSize));
		mBuffers[aCurrentBufferIndex].indexCount = imDrawData->TotalIdxCount;
		mBuffers[aCurrentBufferIndex].indexBuffer.Map();
	}

	// Upload data
	ImDrawVert* vtxDst = (ImDrawVert*)mBuffers[aCurrentBufferIndex].vertexBuffer.mMappedData;
	ImDrawIdx* idxDst = (ImDrawIdx*)mBuffers[aCurrentBufferIndex].indexBuffer.mMappedData;

	for (int n = 0; n < imDrawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = imDrawData->CmdLists[n];
		std::memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		std::memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtxDst += cmd_list->VtxBuffer.Size;
		idxDst += cmd_list->IdxBuffer.Size;
	}

	// Flush to make writes visible to GPU
	mBuffers[aCurrentBufferIndex].vertexBuffer.Flush();
	mBuffers[aCurrentBufferIndex].indexBuffer.Flush();
}

void ImGuiOverlay::Draw(const VkCommandBuffer aVkCommandBuffer, std::uint32_t aCurrentBufferIndex)
{
	ImDrawData* imDrawData = ImGui::GetDrawData();
	std::int32_t vertexOffset = 0;
	std::int32_t indexOffset = 0;

	if ((!imDrawData) || (imDrawData->CmdListsCount == 0))
	{
		return;
	}

	if (mBuffers[aCurrentBufferIndex].vertexBuffer.mVkBuffer == VK_NULL_HANDLE || mBuffers[aCurrentBufferIndex].indexBuffer.mVkBuffer == VK_NULL_HANDLE)
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	vkCmdBindPipeline(aVkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
	vkCmdBindDescriptorSets(aVkCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSet, 0, nullptr);

	mPushConstBlock.scale = Math::Vector2f(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
	mPushConstBlock.translate = Math::Vector2f(-1.0f);
	vkCmdPushConstants(aVkCommandBuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &mPushConstBlock);

	assert(mBuffers[aCurrentBufferIndex].vertexBuffer.mVkBuffer != VK_NULL_HANDLE && mBuffers[aCurrentBufferIndex].indexBuffer.mVkBuffer != VK_NULL_HANDLE);

	VkDeviceSize offsets[1] = {0};
	vkCmdBindVertexBuffers(aVkCommandBuffer, 0, 1, &mBuffers[aCurrentBufferIndex].vertexBuffer.mVkBuffer, offsets);
	vkCmdBindIndexBuffer(aVkCommandBuffer, mBuffers[aCurrentBufferIndex].indexBuffer.mVkBuffer, 0, VK_INDEX_TYPE_UINT16);

	for (std::int32_t i = 0; i < imDrawData->CmdListsCount; i++)
	{
		const ImDrawList* cmd_list = imDrawData->CmdLists[i];
		for (std::int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
			VkRect2D scissorRect{
				.offset = {.x = std::max((std::int32_t)(pcmd->ClipRect.x), 0), .y = std::max((std::int32_t)(pcmd->ClipRect.y), 0) },
				.extent = {.width = (std::uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x), .height = (std::uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y) }
			};
			vkCmdSetScissor(aVkCommandBuffer, 0, 1, &scissorRect);
			vkCmdDrawIndexed(aVkCommandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
			indexOffset += pcmd->ElemCount;
		}
		vertexOffset += cmd_list->VtxBuffer.Size;
	}
}

void ImGuiOverlay::Resize(std::uint32_t aWidth, std::uint32_t aHeight)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)(aWidth), (float)(aHeight));
}

void ImGuiOverlay::FreeResources()
{
	for (Buffers& buffer : mBuffers)
	{
		buffer.vertexBuffer.Destroy();
		buffer.indexBuffer.Destroy();
	}

	vkDestroyImageView(mVulkanDevice->mLogicalVkDevice, mFontImageView, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalVkDevice, mFontImage, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalVkDevice, mFontMemory, nullptr);
	vkDestroySampler(mVulkanDevice->mLogicalVkDevice, mSampler, nullptr);
	vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalVkDevice, mDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(mVulkanDevice->mLogicalVkDevice, mDescriptorPool, nullptr);
	vkDestroyPipelineLayout(mVulkanDevice->mLogicalVkDevice, mPipelineLayout, nullptr);
	vkDestroyPipeline(mVulkanDevice->mLogicalVkDevice, mPipeline, nullptr);
}

void ImGuiOverlay::OnKeyCallback(int aKeycode, int aScancode, int aAction, int /*aMods*/)
{
	if (!ImGui::GetCurrentContext())
		return;

	if (aAction != GLFW_PRESS && aAction != GLFW_RELEASE)
		return;

	ImGuiIO& io = ImGui::GetIO();

	aKeycode = ImGuiOverlayPrivate::TranslateUntranslatedKey(aKeycode, aScancode);

	const ImGuiKey imguiKey = ImGuiOverlayPrivate::KeyToImGuiKey(aKeycode, aScancode);
	io.AddKeyEvent(imguiKey, (aAction == GLFW_PRESS));
	io.SetKeyEventNativeData(imguiKey, aKeycode, aScancode); // To support legacy indexing (<1.87 user code)
}

void ImGuiOverlay::OnWindowFocusCallback(int aFocused)
{
	if (!ImGui::GetCurrentContext())
		return;

	ImGuiIO& io = ImGui::GetIO();
	io.AddFocusEvent(aFocused != 0);
}

void ImGuiOverlay::OnCharCallback(unsigned int aChar)
{
	if (!ImGui::GetCurrentContext())
		return;

	ImGuiIO& io = ImGui::GetIO();
	io.AddInputCharacter(aChar);
}

bool ImGuiOverlay::WantsToCaptureInput() const
{
	const ImGuiIO& io = ImGui::GetIO();
	return io.WantCaptureKeyboard || io.WantCaptureMouse;
}

void ImGuiOverlay::Vec2Text(const char* aLabel, const Math::Vector2f& aVec2)
{
	ImGui::Text("%s %.1f, %.1f", aLabel, aVec2.x, aVec2.y);
}

void ImGuiOverlay::Vec3Text(const char* aLabel, const Math::Vector3f& aVec3)
{
	ImGui::Text("%s %.1f, %.1f, %.1f", aLabel, aVec3.x, aVec3.y, aVec3.z);
}

void ImGuiOverlay::Vec4Text(const char* aLabel, const Math::Vector4f& aVec4)
{
	ImGui::Text("%s %.1f, %.1f, %.1f, %.1f", aLabel, aVec4.x, aVec4.y, aVec4.z, aVec4.w);
}

void ImGuiOverlay::Mat4Text(const char* aLabel, const Math::Matrix4f& aMat4)
{
	Math::Vector3f scale;
	Math::Quaternionf rotation;
	Math::Vector3f translation;
	if (Math::Decompose(aMat4, scale, rotation, translation))
	{
		ImGui::Text("%s position %.1f, %.1f, %.1f", aLabel, translation.x, translation.y, translation.z);
		ImGui::Text("%s rotation %.1f, %.1f, %.1f", aLabel, rotation.x, rotation.y, rotation.z);
		ImGui::Text("%s scale %.1f, %.1f, %.1f", aLabel, scale.x, scale.y, scale.z);
	}
}
