#pragma once

class VulkanExample;

class Engine
{
public:
	Engine();
	~Engine();

	void Start();
	void Run();
	void Shutdown();

private:
	VulkanExample* mVulkanExample;
	float mDeltaTime;
	float mFixedDeltaTime;
	float mTimeScale;
};
