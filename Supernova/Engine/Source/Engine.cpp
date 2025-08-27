#include "Engine.hpp"

#include "VulkanRenderer.hpp"

#include <chrono>
#include <ratio>

Engine::Engine()
	: mVulkanRenderer(nullptr)
	, mFixedDeltaTime(0.0f)
	, mTimeScale(0.25f)
	, mDeltaTime(0.0f)
{
}

Engine::~Engine()
{
	delete mVulkanRenderer;
}

void Engine::Start()
{
	mVulkanRenderer = new VulkanRenderer();
	mVulkanRenderer->InitializeRenderer();
}

void Engine::Run()
{
	mVulkanRenderer->PrepareUpdate();

	while (!mVulkanRenderer->ShouldClose())
	{
		const std::chrono::steady_clock::time_point startTime = std::chrono::high_resolution_clock::now();

		mVulkanRenderer->UpdateRenderer(mDeltaTime);

		const std::chrono::steady_clock::time_point endTime = std::chrono::high_resolution_clock::now();
		const float deltaTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
		mDeltaTime = deltaTimeMs / 1000.0f;

		if (!mVulkanRenderer->IsPaused())
		{
			mFixedDeltaTime += mTimeScale * mDeltaTime;
			if (mFixedDeltaTime > 1.0f)
			{
				mFixedDeltaTime -= 1.0f;
			}
		}
	}
}

void Engine::Shutdown()
{
	mVulkanRenderer->DestroyRenderer();
}
