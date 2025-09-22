#pragma once

#include <filesystem>

namespace FileLoader
{
	bool IsFileValid(const std::filesystem::path& aPath);
	unsigned char* LoadImage(const std::filesystem::path& aPath, int& aWidth, int& aHeight, int& aNumberOfComponents);
}
