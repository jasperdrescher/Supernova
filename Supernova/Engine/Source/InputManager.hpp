#pragma once

#include "InputKeys.hpp"

#include <map>

enum class MouseButtons
{
	Undefined,
	Left,
	Right,
	Middle
};

class InputManager
{
public:
	static InputManager& GetInstance()
	{
		static InputManager instance;
		return instance;
	}

	InputManager(InputManager const&) = delete;
	void operator=(InputManager const&) = delete;

	void OnKeyAction(int aKey, int aScancode, bool aIsKeyDown, int aMode);
	void OnCursorAction(double aXPosition, double aYPosition);
	void OnScrollAction(double aXOffset, double aYOffset);
	void OnMouseButtonAction(int aButton, int aAction, int aModifier);

	bool GetIsKeyDown(Key aKey) const;
	bool GetIsMouseButtonDown(MouseButtons aMouseButton) const;

	float myCursorXPosition;
	float myCursorYPosition;
	float myScrollXOffset;
	float myScrollYOffset;

private:
	InputManager();

	Key GetTranslatedKey(int aKey) const;
	MouseButtons GetTranslatedMouseButton(int aButton) const;

private:
	std::map<Key, bool> myKeys;
	std::map<MouseButtons, bool> myMouseButtons;
};
