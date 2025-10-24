#include "Window.hpp"

#include "FileLoader.hpp"
#include "ImGuiOverlay.hpp"
#include "Input/InputManager.hpp"
#include "Profiler/SimpleProfiler.hpp"
#include "Vulkantools.hpp"

#define GLFW_EXCLUDE_API
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

namespace WindowLocal
{
	static void GLFWErrorCallback(int aError, const char* aDescription)
	{
		std::cerr << "GLFW error: " << aError << " " << aDescription << std::endl;
	}
}

Window::Window()
	: mGLFWWindow{nullptr}
	, mIconPath{"Textures/Supernova.png"}
	, mShouldClose{false}
{
}

Window::~Window()
{
	glfwDestroyWindow(mGLFWWindow);
	glfwTerminate();
}

void Window::InitializeWindow(const std::string& aApplicationName)
{
	if (glfwInit() != GLFW_TRUE)
	{
		throw std::runtime_error("Failed to init GLFW");
	}

	glfwSetErrorCallback(WindowLocal::GLFWErrorCallback);

	if (glfwVulkanSupported() != GLFW_TRUE)
	{
		throw std::runtime_error("Failed to init Vulkan");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	mGLFWWindow = glfwCreateWindow(mWindowProperties.mWindowWidth, mWindowProperties.mWindowHeight, aApplicationName.c_str(), nullptr, nullptr);
	if (!mGLFWWindow)
	{
		glfwTerminate();
		throw std::runtime_error("Failed to create a window");
	}

	glfwSetWindowUserPointer(mGLFWWindow, this);
	glfwSetKeyCallback(mGLFWWindow, KeyCallback);
	glfwSetCharCallback(mGLFWWindow, CharCallback);
	glfwSetMouseButtonCallback(mGLFWWindow, MouseButtonCallback);
	glfwSetCursorPosCallback(mGLFWWindow, CursorPositionCallback);
	glfwSetScrollCallback(mGLFWWindow, ScrollCallback);
	glfwSetWindowFocusCallback(mGLFWWindow, WindowFocusCallback);
	glfwSetFramebufferSizeCallback(mGLFWWindow, FramebufferResizeCallback);
	glfwSetWindowSizeCallback(mGLFWWindow, WindowResizeCallback);
	glfwSetWindowIconifyCallback(mGLFWWindow, WindowMinimizedCallback);
	glfwSetInputMode(mGLFWWindow, GLFW_STICKY_KEYS, GLFW_TRUE);
	glfwSetInputMode(mGLFWWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	if (glfwRawMouseMotionSupported() == GLFW_TRUE)
		glfwSetInputMode(mGLFWWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	int iconWidth = 0;
	int iconHeight = 0;
	int iconNumberOfComponents = 0;
	unsigned char* iconSource = FileLoader::LoadImage(FileLoader::GetEngineResourcesPath() / mIconPath, iconWidth, iconHeight, iconNumberOfComponents);
	SetWindowIcon(iconSource, iconWidth, iconHeight);

	glfwPollEvents();

	int major, minor, revision;
	glfwGetVersion(&major, &minor, &revision);

	std::cout << std::format("GLFW v{}.{}.{}", major, minor, revision) << std::endl;
}

void Window::CreateWindowSurface(VkInstance* aVkInstance, VkSurfaceKHR* aVkSurface)
{
	VK_CHECK_RESULT(glfwCreateWindowSurface(*aVkInstance, mGLFWWindow, nullptr, aVkSurface));
}

void Window::UpdateWindow()
{
	SIMPLE_PROFILER_PROFILE_SCOPE("Window::UpdateWindow");

	glfwPollEvents();

	mShouldClose = glfwWindowShouldClose(mGLFWWindow);
}

void Window::SetWindowSize(int aWidth, int aHeight)
{
	if (mWindowProperties.mIsMinimized)
		return;

	if (aWidth <= 0 || aHeight <= 0)
		return;

	mWindowProperties.mWindowWidth = aWidth;
	mWindowProperties.mWindowHeight = aHeight;
}

void Window::OnFramebufferResizeProcessed()
{
	mWindowProperties.mIsFramebufferResized = false;
}

std::vector<const char*> Window::GetGlfwRequiredExtensions()
{
	std::uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	if (glfwExtensionCount == 0)
		throw std::runtime_error("Failed to find required GLFW extensions");

	std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	return requiredExtensions;
}

void Window::KeyCallback(GLFWwindow* aWindow, int aKey, int aScancode, int aAction, int aMode)
{
	Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(aWindow));
	if (aKey == GLFW_KEY_ESCAPE && aAction != GLFW_RELEASE)
	{
		glfwSetWindowShouldClose(window->mGLFWWindow, GLFW_TRUE);
	}

	Input::InputManager::GetInstance().OnKeyAction(aKey, aScancode, aAction != GLFW_RELEASE, aMode);
	ImGuiOverlay::OnKeyCallback(aKey, aScancode, aAction, aMode);
}

void Window::MouseButtonCallback(GLFWwindow* /*window*/, int aButton, int aAction, int aMode)
{
	Input::InputManager::GetInstance().OnMouseButtonAction(aButton, aAction, aMode);
}

void Window::CursorPositionCallback(GLFWwindow* /*aWindow*/, double aX, double aY)
{
	Input::InputManager::GetInstance().OnCursorAction(aX, aY);
}

void Window::ScrollCallback(GLFWwindow* /*aWindow*/, double aX, double aY)
{
	Input::InputManager::GetInstance().OnScrollAction(aX, aY);
}

void Window::FramebufferResizeCallback(GLFWwindow* aWindow, int /*aWidth*/, int /*aHeight*/)
{
	Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(aWindow));
	if (window->mWindowProperties.mIsMinimized)
		return;

	window->mWindowProperties.mIsFramebufferResized = true;
}

void Window::WindowResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight)
{
	Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(aWindow));
	window->SetWindowSize(aWidth, aHeight);
}

void Window::WindowMinimizedCallback(GLFWwindow* aWindow, int aValue)
{
	Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(aWindow));
	window->mWindowProperties.mIsMinimized = aValue;
}

void Window::CharCallback(GLFWwindow* /*aWindow*/, unsigned int aChar)
{
	ImGuiOverlay::OnCharCallback(aChar);
}

void Window::WindowFocusCallback(GLFWwindow* aWindow, int aFocused)
{
	Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(aWindow));
	window->mWindowProperties.mIsFocused = aFocused;

	ImGuiOverlay::OnWindowFocusCallback(aFocused);
}

void Window::SetWindowIcon(unsigned char* aSource, int aWidth, int aHeight) const
{
	GLFWimage processIcon[1]{};
	processIcon[0].pixels = aSource;
	processIcon[0].width = aWidth;
	processIcon[0].height = aHeight;
	glfwSetWindowIcon(mGLFWWindow, 1, processIcon);
}

float Window::GetContentScaleForMonitor() const
{
	float scaleX = 0.0f;
	float scaleY = 0.0f;
	glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &scaleX, &scaleY);
	return scaleX;
}
