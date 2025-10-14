#pragma once

namespace Time
{
	struct Timer;
}

struct EngineProperties;
class Window;
class VulkanRenderer;

class Engine
{
public:
	Engine();
	~Engine();

	void Start();
	void Run();

private:
	EngineProperties* mEngineProperties;
	Window* mVulkanWindow;
	VulkanRenderer* mVulkanRenderer;
	Time::Timer* mTimer;
	float mDeltaTime;
	float mFixedDeltaTime;
	float mTimeScale;
};
