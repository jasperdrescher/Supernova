#pragma once

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
	float mDeltaTime;
	float mFixedDeltaTime;
	float mTimeScale;
};
