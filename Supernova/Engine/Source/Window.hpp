#pragma once

#include <filesystem>
#include <vector>

typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
typedef struct GLFWwindow GLFWwindow;

struct EngineProperties;

class Window
{
public:
	Window(EngineProperties* aEngineProperties);
	~Window();

	void InitializeWindow();
	void CreateWindowSurface(VkInstance* aVkInstance, VkSurfaceKHR* aVkSurface);
	void UpdateWindow();

	void SetWindowSize(int aWidth, int aHeight);

	bool ShouldClose() const { return mShouldClose; }
	std::vector<const char*> GetGlfwRequiredExtensions();

private:
	static void KeyCallback(GLFWwindow* aWindow, int aKey, int aScancode, int aAction, int aMode);
	static void FramebufferResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
	static void WindowResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
	static void WindowMinimizedCallback(GLFWwindow* aWindow, int aValue);

	void SetWindowIcon(unsigned char* aSource, int aWidth, int aHeight) const;

	std::filesystem::path mIconPath;
	bool mShouldClose;
	EngineProperties* mEngineProperties;
	GLFWwindow* mGLFWWindow;
};
