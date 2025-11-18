#pragma once

#include <filesystem>

namespace FileLoader
{
	bool IsFileValid(const std::filesystem::path& aPath);
	unsigned char* LoadImage(const std::filesystem::path& aPath, int& aWidth, int& aHeight, int& aNumberOfComponents);
	void PrintWorkingDirectory();

	std::filesystem::path GetEngineResourcesPath();
	static std::filesystem::path gEnginePath = "Engine/";
	static std::filesystem::path gResourcesPath = "Resources/";
	static std::filesystem::path gShadersPath = "Shaders/GLSL/";
	static std::filesystem::path gFontPath = "Fonts/";
	static std::filesystem::path gModelsPath = "Models/";
	static std::filesystem::path gTexturesPath = "Textures/";
}
