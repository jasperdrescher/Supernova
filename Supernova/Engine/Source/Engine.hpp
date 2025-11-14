#pragma once

#include <memory>

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
	std::shared_ptr<EngineProperties> mEngineProperties;
	std::shared_ptr<Window> mVulkanWindow;
	std::unique_ptr<VulkanRenderer> mVulkanRenderer;
	std::unique_ptr<Time::Timer> mTimer;
	float mDeltaTime;
	float mFixedDeltaTime;
	float mTimeScale;
};
