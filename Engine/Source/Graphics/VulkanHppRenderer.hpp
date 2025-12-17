#include <memory>

struct EngineProperties;
class Window;

class VulkanHppRenderer
{
public:
	VulkanHppRenderer(const std::shared_ptr<EngineProperties>& aEngineProperties, const std::shared_ptr<Window>& aWindow);

	void InitializeRenderer();

private:
	std::weak_ptr<EngineProperties> mEngineProperties;
	std::weak_ptr<Window> mWindow;
};
