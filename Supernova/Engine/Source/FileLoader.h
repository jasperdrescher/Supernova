#pragma once

#include <string>

namespace FileLoader
{
	static bool IsFileValid(const std::string& aPath);
	unsigned char* LoadImage(const std::string& aFilename, int& aWidth, int& aHeight, int& aNumberOfComponents);
}
