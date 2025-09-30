#include "Window.hpp"

#include "EngineProperties.hpp"
#include "FileLoader.hpp"
#include "InputManager.hpp"
#include "Vulkantools.hpp"

#define GLFW_EXCLUDE_API
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cstdint>
#include <format>
#include <stdexcept>

namespace WindowLocal
{
	static void GLFWErrorCallback(int aError, const char* aDescription)
	{
		std::cerr << "GLFW error: " << aError << " " << aDescription << std::endl;
	}
}

Window::Window(EngineProperties* aEngineProperties)
	: mEngineProperties{aEngineProperties}
	, mGLFWWindow{nullptr}
	, mIconPath{"Textures/Supernova.png"}
	, mShouldClose{false}
{
}

Window::~Window()
{
	glfwDestroyWindow(mGLFWWindow);
	glfwTerminate();
}

void Window::InitializeWindow()
{
	if (!glfwInit())
	{
		throw std::runtime_error("Failed to init GLFW");
	}

	glfwSetErrorCallback(WindowLocal::GLFWErrorCallback);

	if (!glfwVulkanSupported())
	{
		throw std::runtime_error("Failed to init Vulkan");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	mGLFWWindow = glfwCreateWindow(mEngineProperties->mWindowWidth, mEngineProperties->mWindowHeight, mEngineProperties->mApplicationName.c_str(), nullptr, nullptr);
	if (!mGLFWWindow)
	{
		glfwTerminate();
		throw std::runtime_error("Failed to create a window");
	}

	glfwSetWindowUserPointer(mGLFWWindow, this);
	glfwSetKeyCallback(mGLFWWindow, KeyCallback);
	glfwSetCursorPosCallback(mGLFWWindow, CursorPositionCallback);
	glfwSetFramebufferSizeCallback(mGLFWWindow, FramebufferResizeCallback);
	glfwSetWindowSizeCallback(mGLFWWindow, WindowResizeCallback);
	glfwSetWindowIconifyCallback(mGLFWWindow, WindowMinimizedCallback);
	glfwSetInputMode(mGLFWWindow, GLFW_STICKY_KEYS, GLFW_TRUE);
	glfwSetInputMode(mGLFWWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(mGLFWWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	glfwSetWindowTitle(mGLFWWindow, mEngineProperties->mApplicationName.c_str());

	int iconWidth = 0;
	int iconHeight = 0;
	int iconNumberOfComponents = 0;
	unsigned char* iconSource = FileLoader::LoadImage(VulkanTools::gResourcesPath / mIconPath, iconWidth, iconHeight, iconNumberOfComponents);
	SetWindowIcon(iconSource, iconWidth, iconHeight);

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
	glfwPollEvents();

	mEngineProperties->mIsFocused = glfwGetWindowAttrib(mGLFWWindow, GLFW_FOCUSED);
	mShouldClose = glfwWindowShouldClose(mGLFWWindow);
}

void Window::SetWindowSize(int aWidth, int aHeight)
{
	mEngineProperties->mWindowWidth = aWidth;
	mEngineProperties->mWindowHeight = aHeight;
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

	InputManager::GetInstance().OnKeyAction(aKey, aScancode, aAction != GLFW_RELEASE, aMode);
}

void Window::CursorPositionCallback(GLFWwindow* /*aWindow*/, double aX, double aY)
{
	InputManager::GetInstance().OnCursorAction(aX, aY);
}

void Window::FramebufferResizeCallback(GLFWwindow* aWindow, int /*aWidth*/, int /*aHeight*/)
{
	Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(aWindow));
	if (window->mEngineProperties->mIsMinimized || !window->mEngineProperties->mIsRendererPrepared)
		return;

	window->mEngineProperties->mIsFramebufferResized = true;
}

void Window::WindowResizeCallback(GLFWwindow* aWindow, int aWidth, int aHeight)
{
	Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(aWindow));
	if (window->mEngineProperties->mIsMinimized)
		return;

	if (aWidth <= 0 || aHeight <= 0)
		return;

	window->SetWindowSize(aWidth, aHeight);
}

void Window::WindowMinimizedCallback(GLFWwindow* aWindow, int aValue)
{
	Window* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(aWindow));
	window->mEngineProperties->mIsMinimized = aValue;
	window->mEngineProperties->mIsPaused = aValue;
}

void Window::SetWindowIcon(unsigned char* aSource, int aWidth, int aHeight) const
{
	GLFWimage processIcon[1]{};
	processIcon[0].pixels = aSource;
	processIcon[0].width = aWidth;
	processIcon[0].height = aHeight;
	glfwSetWindowIcon(mGLFWWindow, 1, processIcon);
}
