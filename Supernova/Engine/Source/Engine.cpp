#include "Engine.hpp"

#include "EngineProperties.hpp"
#include "FileLoader.hpp"
#include "Graphics/VulkanRenderer.hpp"
#include "Graphics/Window.hpp"

#include <chrono>
#include <ratio>

Engine::Engine()
	: mEngineProperties{nullptr}
	, mVulkanWindow{nullptr}
	, mVulkanRenderer{nullptr}
	, mFixedDeltaTime{0.0f}
	, mTimeScale{0.25f}
	, mDeltaTime{0.0f}
{
	mEngineProperties = new EngineProperties();
	mVulkanWindow = new Window(mEngineProperties);
	mVulkanRenderer = new VulkanRenderer(mEngineProperties, mVulkanWindow);
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
		const std::chrono::steady_clock::time_point startTime = std::chrono::high_resolution_clock::now();

		mVulkanRenderer->UpdateRenderer(mDeltaTime);

		const std::chrono::steady_clock::time_point endTime = std::chrono::high_resolution_clock::now();
		const float deltaTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
		mDeltaTime = deltaTimeMs / 1000.0f;

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
