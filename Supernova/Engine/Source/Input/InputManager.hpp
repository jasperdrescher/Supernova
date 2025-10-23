#pragma once

#include "InputKeys.hpp"

#include <glm/vec2.hpp>
#include <map>

namespace Input
{
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

		void FlushInput();

		void OnKeyAction(int aKey, int aScancode, bool aIsKeyDown, int aMode);
		void OnCursorAction(double aXPosition, double aYPosition);
		void OnScrollAction(double aXOffset, double aYOffset);
		void OnMouseButtonAction(int aButton, int aAction, int aModifier);

		const glm::vec2& GetMousePosition() const { return mMousePosition; }
		const glm::vec2& GetScrollOffset() const { return mScrollOffset; }
		glm::vec2 GetMousePositionDelta() const { return mMousePosition - mPreviousMousePosition; }
		bool GetIsKeyDown(Key aKey) const;
		bool GetIsMouseButtonDown(MouseButton aMouseButton) const;

	private:
		InputManager() {}

		Key GetTranslatedKey(int aKey) const;
		MouseButton GetTranslatedMouseButton(int aButton) const;

		std::map<Key, bool> myKeys{};
		std::map<MouseButton, bool> myMouseButtons{};
		glm::vec2 mPreviousMousePosition{};
		glm::vec2 mMousePosition{};
		glm::vec2 mScrollOffset{};
	};
}
