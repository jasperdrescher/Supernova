#include "Engine.hpp"

#include "EngineProperties.hpp"
#include "FileLoader.hpp"
#include "Graphics/VulkanRenderer.hpp"
#include "Graphics/Window.hpp"
#include "Timer.hpp"

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
	mVulkanWindow = new Window(mEngineProperties);
	mVulkanRenderer = new VulkanRenderer(mEngineProperties, mVulkanWindow);
	mTimer = new Time::Timer();
}

Engine::~Engine()
{
	delete mVulkanRenderer;
	delete mVulkanWindow;
	delete mEngineProperties;
}

void Engine::Start()
{
	FileLoader::PrintWorkingDirectory();

	mVulkanWindow->InitializeWindow();
	mVulkanRenderer->InitializeRenderer();
}

void Engine::Run()
{
	mVulkanRenderer->PrepareUpdate();

	while (!mVulkanWindow->ShouldClose())
	{
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
