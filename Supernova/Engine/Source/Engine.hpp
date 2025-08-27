#pragma once

class VulkanRenderer;

class Engine
{
public:
	Engine();
	~Engine();

	void Start();
	void Run();
	void Shutdown();

private:
	VulkanRenderer* mVulkanRenderer;
	float mDeltaTime;
	float mFixedDeltaTime;
	float mTimeScale;
};
