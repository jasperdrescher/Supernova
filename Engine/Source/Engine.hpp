#pragma once

#include <memory>

namespace Time
{
	struct Timer;
}

struct EngineProperties;
class Window;

#if VULKAN_C
class VulkanCRenderer;
#endif

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
#if VULKAN_C
	std::unique_ptr<VulkanCRenderer> mVulkanCRenderer;
#endif
	std::unique_ptr<Time::Timer> mTimer;
	float mDeltaTime;
	float mFixedDeltaTime;
	float mTimeScale;
};
