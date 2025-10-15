#pragma once

#include "InputKeys.hpp"

#include <map>
#include <glm/vec2.hpp>

namespace Input
{
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

		const glm::vec2& GetMousePosition() const { return mMousePosition; }
		const glm::vec2& GetScrollOffset() const { return mScrollOffset; }
		bool GetIsKeyDown(Key aKey) const;
		bool GetIsMouseButtonDown(MouseButtons aMouseButton) const;

	private:
		InputManager() {}

		Key GetTranslatedKey(int aKey) const;
		MouseButtons GetTranslatedMouseButton(int aButton) const;

	private:
		std::map<Key, bool> myKeys{};
		std::map<MouseButtons, bool> myMouseButtons{};
		glm::vec2 mMousePosition{};
		glm::vec2 mScrollOffset{};
	};
}
