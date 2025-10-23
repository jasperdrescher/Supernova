#pragma once

#include <filesystem>
#include <string>
#include <vector>

typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
typedef struct GLFWwindow GLFWwindow;

struct EngineProperties;

struct WindowProperties
{
	WindowProperties() : mWindowWidth{1280}, mWindowHeight{720}, mIsFocused{false}, mIsMinimized{false}, mIsFramebufferResized{false} {}

	int mWindowWidth;
	int mWindowHeight;
	bool mIsFocused;
	bool mIsMinimized;
	bool mIsFramebufferResized;
};

class Window
{
public:
	Window();
	~Window();

	void InitializeWindow(const std::string& aApplicationName);
	void CreateWindowSurface(VkInstance* aVkInstance, VkSurfaceKHR* aVkSurface);
	void UpdateWindow();

	void SetWindowSize(int aWidth, int aHeight);
	void OnFramebufferResizeProcessed();

	bool ShouldClose() const { return mShouldClose; }
	const WindowProperties& GetWindowProperties() const { return mWindowProperties; }
	std::vector<const char*> GetGlfwRequiredExtensions();
	float GetContentScaleForMonitor() const;

private:
	static void KeyCallback(GLFWwindow* aWindow, int aKey, int aScancode, int aAction, int aMode);
	static void MouseButtonCallback(GLFWwindow* window, int aButton, int aAction, int aMode);
	static void CursorPositionCallback(GLFWwindow* aWindow, double aX, double aY);
	static void ScrollCallback(GLFWwindow* window, double aX, double aY);
	static void FramebufferResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
	static void WindowResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight);
	static void WindowMinimizedCallback(GLFWwindow* aWindow, int aValue);

	void SetWindowIcon(unsigned char* aSource, int aWidth, int aHeight) const;

	WindowProperties mWindowProperties;
	std::filesystem::path mIconPath;
	bool mShouldClose;
	GLFWwindow* mGLFWWindow;
};
