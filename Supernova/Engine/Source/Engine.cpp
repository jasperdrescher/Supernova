#include "Engine.hpp"

#include "Renderer.hpp"

#include <chrono>
#include <ratio>

Engine::Engine()
	: mVulkanExample(nullptr)
	, mFixedDeltaTime(0.0f)
	, mTimeScale(0.25f)
	, mDeltaTime(0.0f)
{
}

Engine::~Engine()
{
	delete mVulkanExample;
}

void Engine::Start()
{
	mVulkanExample = new VulkanExample();
	mVulkanExample->InitializeRenderer();
}

void Engine::Run()
{
	mVulkanExample->PrepareUpdate();

	while (!mVulkanExample->ShouldClose())
	{
		const std::chrono::steady_clock::time_point startTime = std::chrono::high_resolution_clock::now();

		mVulkanExample->UpdateRenderer(mDeltaTime);

		const std::chrono::steady_clock::time_point endTime = std::chrono::high_resolution_clock::now();
		const float deltaTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
		mDeltaTime = deltaTimeMs / 1000.0f;

		if (!mVulkanExample->IsPaused())
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
	mVulkanExample->DestroyRenderer();
}
