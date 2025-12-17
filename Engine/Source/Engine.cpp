#include "Engine.hpp"

#include "EngineProperties.hpp"
#include "FileLoader.hpp"
#ifdef VULKAN_C
#include "Graphics/VulkanCRenderer.hpp"
#endif
#include "Graphics/Window.hpp"
#include "Profiler/SimpleProfiler.hpp"
#include "Timer.hpp"

#include <format>
#include <iostream>
#include <memory>

Engine::Engine()
	: mEngineProperties{nullptr}
	, mVulkanWindow{nullptr}
#ifdef VULKAN_C
	, mVulkanCRenderer{nullptr}
#endif
	, mTimer{nullptr}
	, mFixedDeltaTime{0.0f}
	, mTimeScale{0.25f}
	, mDeltaTime{0.0f}
{
	mEngineProperties = std::make_shared<EngineProperties>();
	mVulkanWindow = std::make_shared<Window>();

#ifdef VULKAN_C
	mVulkanCRenderer = std::make_unique<VulkanCRenderer>(mEngineProperties, mVulkanWindow);
#endif
	mTimer = std::make_unique<Time::Timer>();

	mEngineProperties->mApplicationName = "Supernova Editor";
	mEngineProperties->mEngineName = "Supernova Engine";
	mEngineProperties->mEngineMinorVersion = 1;
}

Engine::~Engine()
{
}

void Engine::Start()
{
	std::cout << std::format("{} v{}.{}.{}", mEngineProperties->mEngineName, mEngineProperties->mEngineMajorVersion, mEngineProperties->mEngineMinorVersion, mEngineProperties->mEnginePatchVersion) << std::endl;

	FileLoader::PrintWorkingDirectory();

	mVulkanWindow->InitializeWindow(mEngineProperties->mApplicationName);

#ifdef VULKAN_C
	mVulkanCRenderer->InitializeRenderer();
#endif
}

void Engine::Run()
{
#ifdef VULKAN_C
	mVulkanCRenderer->PrepareUpdate();
#endif

	while (!mVulkanWindow->ShouldClose())
	{
		SIMPLE_PROFILER_PROFILE_SCOPE("Engine::Run");

		mTimer->StartTimer();

#ifdef VULKAN_C
		mVulkanCRenderer->UpdateRenderer(mDeltaTime);
#endif

		mTimer->EndTimer();

		mDeltaTime = static_cast<float>(mTimer->GetDurationSeconds());

		if (!mEngineProperties->mIsPaused)
		{
			mFixedDeltaTime += mTimeScale * mDeltaTime;
			if (mFixedDeltaTime > 1.0f)
			{
				mFixedDeltaTime -= 1.0f;
			}
		}
	}

#ifdef VULKAN_C
	mVulkanCRenderer->EndUpdate();
#endif
}
