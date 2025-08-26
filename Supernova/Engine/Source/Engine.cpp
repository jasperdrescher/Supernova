#include "Engine.hpp"

#include "Renderer.hpp"

Engine::Engine()
	: mVulkanExample(nullptr)
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
	while (!mVulkanExample->ShouldClose())
	{
		mVulkanExample->UpdateRenderer();
	}
}

void Engine::Shutdown()
{
	mVulkanExample->DestroyRenderer();
}
