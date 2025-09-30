#include "EngineProperties.hpp"

EngineProperties::EngineProperties()
	: mAPIVersion{0}
	, mWindowWidth{1280}
	, mWindowHeight{720}
	, mIsMinimized{false}
	, mIsFocused{false}
	, mIsPaused{false}
	, mIsFramebufferResized{false}
	, mIsRendererPrepared{false}
	, mIsVSyncEnabled{false}
	, mIsValidationEnabled{false}
{
}
