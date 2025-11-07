#include "Engine.hpp"

#include "EngineProperties.hpp"
#include "FileLoader.hpp"
#include "Graphics/VulkanRenderer.hpp"
#include "Graphics/Window.hpp"
#include "Profiler/SimpleProfiler.hpp"
#include "Timer.hpp"

#include <format>
#include <iostream>
#include <memory>

Engine::Engine()
	: mEngineProperties{nullptr}
	, mVulkanWindow{nullptr}
	, mVulkanRenderer{nullptr}
	, mTimer{nullptr}
	, mFixedDeltaTime{0.0f}
	, mTimeScale{0.25f}
	, mDeltaTime{0.0f}
{
	mEngineProperties = new EngineProperties();
	mVulkanWindow = std::make_shared<Window>();
	mVulkanRenderer = std::make_unique<VulkanRenderer>(mEngineProperties, mVulkanWindow);
	mTimer = std::make_unique<Time::Timer>();

	mEngineProperties->mApplicationName = "Supernova Editor";
	mEngineProperties->mEngineName = "Supernova Engine";
	mEngineProperties->mEngineMinorVersion = 1;
}

Engine::~Engine()
{
	delete mEngineProperties;
}

void Engine::Start()
{
	std::cout << std::format("{} v{}.{}.{}", mEngineProperties->mEngineName, mEngineProperties->mEngineMajorVersion, mEngineProperties->mEngineMinorVersion, mEngineProperties->mEnginePatchVersion) << std::endl;

	FileLoader::PrintWorkingDirectory();

	mVulkanWindow->InitializeWindow(mEngineProperties->mApplicationName);
	mVulkanRenderer->InitializeRenderer();
}

void Engine::Run()
{
	mVulkanRenderer->PrepareUpdate();

	while (!mVulkanWindow->ShouldClose())
	{
		SIMPLE_PROFILER_PROFILE_SCOPE("Engine::Run");

		mTimer->StartTimer();

		mVulkanRenderer->UpdateRenderer(mDeltaTime);

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

	mVulkanRenderer->EndUpdate();
}
