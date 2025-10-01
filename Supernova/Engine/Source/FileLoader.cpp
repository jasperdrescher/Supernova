#include "FileLoader.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <iostream>

namespace FileLoader
{
	bool IsFileValid(const std::filesystem::path& aPath)
	{
		return std::filesystem::exists(aPath);
	}

	unsigned char* LoadImage(const std::filesystem::path& aPath, int& aWidth, int& aHeight, int& aNumberOfComponents)
	{
		const bool isFileValid = FileLoader::IsFileValid(aPath);
		if (!isFileValid)
		{
			throw std::runtime_error(std::format("Could not find file: {}", aPath.generic_string()));
		}

		unsigned char* image = stbi_load(aPath.generic_string().c_str(), &aWidth, &aHeight, &aNumberOfComponents, STBI_default);
		if (!image)
		{
			stbi_image_free(image);
			return nullptr;
		}

		return image;
	}

	void PrintWorkingDirectory()
	{
		std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;
	}

	std::filesystem::path GetEngineResourcesPath()
	{
		return std::filesystem::current_path().parent_path() / gEnginePath / gResourcesPath;
	}
}
