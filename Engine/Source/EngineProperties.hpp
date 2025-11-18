#pragma once

#include <cstdint>
#include <string>

struct EngineProperties
{
	EngineProperties();

	std::string mWindowTitle;
	std::string mApplicationName;
	std::string mEngineName;
	std::uint32_t mAPIVersion;
	std::uint32_t mEngineMajorVersion;
	std::uint32_t mEngineMinorVersion;
	std::uint32_t mEnginePatchVersion;
	bool mIsPaused;
	bool mIsRendererPrepared;
	bool mIsVSyncEnabled;
	bool mIsValidationEnabled;
};
