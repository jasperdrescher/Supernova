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
