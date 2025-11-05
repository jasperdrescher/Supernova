#pragma once

#include <cstdint>
#include <filesystem>

struct VulkanDevice;

enum FileLoadingFlags : unsigned int
{
	None = 0,
	PreTransformVertices = 1 << 0,
	PreMultiplyVertexColors = 1 << 1,
	FlipY = 1 << 2,
	DontLoadImages = 1 << 3
};

class ModelLoader
{
public:
	void LoadModel(const std::filesystem::path& aPath, std::uint32_t aFileLoadingFlags = FileLoadingFlags::None, float aScale = 1.0f);

private:

};
