#pragma once

#include <memory>

namespace Time
{
	struct Timer;
}

struct EngineProperties;
class Window;

#ifdef VULKAN_C
class VulkanCRenderer;
#else
class VulkanHppRenderer;
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
#ifdef VULKAN_C
	std::unique_ptr<VulkanCRenderer> mVulkanCRenderer;
#else
	std::unique_ptr<VulkanHppRenderer> mVulkanHppRenderer;
#endif
	std::unique_ptr<Time::Timer> mTimer;
	float mDeltaTime;
	float mFixedDeltaTime;
	float mTimeScale;
};
