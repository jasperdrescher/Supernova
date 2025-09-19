#include "FileLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cassert>
#include <filesystem>

namespace FileLoader
{
	static bool IsFileValid(const std::filesystem::path& aPath)
	{
		return std::filesystem::exists(aPath);
	}

	unsigned char* LoadImage(const std::filesystem::path& aPath, int& aWidth, int& aHeight, int& aNumberOfComponents)
	{
		const bool isFileValid = FileLoader::IsFileValid(aPath);
		assert(isFileValid);
		if (!isFileValid)
			return nullptr;

		unsigned char* image = stbi_load(aPath.generic_string().c_str(), &aWidth, &aHeight, &aNumberOfComponents, STBI_default);
		if (!image)
		{
			stbi_image_free(image);
			return nullptr;
		}

		return image;
	}
}
