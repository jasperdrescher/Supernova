#include "EngineProperties.hpp"

EngineProperties::EngineProperties()
	: mAPIVersion{0}
	, mEngineMajorVersion{0}
	, mEngineMinorVersion{0}
	, mEnginePatchVersion{0}
	, mIsPaused{false}
	, mIsRendererPrepared{false}
	, mIsVSyncEnabled{false}
	, mIsValidationEnabled{false}
{
}
