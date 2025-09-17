#include "FileLoader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cassert>
#include <filesystem>
#include <string>

static bool FileLoader::IsFileValid(const std::string& aPath)
{
	return std::filesystem::exists(aPath);
}

unsigned char* FileLoader::LoadImage(const std::string& aFilename, int& aWidth, int& aHeight, int& aNumberOfComponents)
{
	const bool isFileValid = FileLoader::IsFileValid(aFilename);
	assert(isFileValid);
	if (!isFileValid)
		return nullptr;

	unsigned char* image = stbi_load(aFilename.c_str(), &aWidth, &aHeight, &aNumberOfComponents, STBI_default);
	if (!image)
	{
		stbi_image_free(image);
		return nullptr;
	}

	return image;
}
