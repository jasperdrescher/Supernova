#include "EngineProperties.hpp"

EngineProperties::EngineProperties()
	: mAPIVersion{0}
	, mWindowWidth{1280}
	, mWindowHeight{720}
	, mEngineMajorVersion{0}
	, mEngineMinorVersion{0}
	, mEnginePatchVersion{0}
	, mIsMinimized{false}
	, mIsFocused{false}
	, mIsPaused{false}
	, mIsFramebufferResized{false}
	, mIsRendererPrepared{false}
	, mIsVSyncEnabled{false}
	, mIsValidationEnabled{false}
{
}
