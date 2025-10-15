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
	int mWindowWidth;
	int mWindowHeight;
	bool mIsMinimized;
	bool mIsFocused;
	bool mIsPaused;
	bool mIsFramebufferResized;
	bool mIsRendererPrepared;
	bool mIsVSyncEnabled;
	bool mIsValidationEnabled;
};
