#pragma once

#include "Core/ThreadSafeSingleton.hpp"
#include "InputKeys.hpp"
#include "Math/Types.hpp"

#include <map>

namespace Input
{
	class InputManager : public ThreadSafeSingleton<InputManager>
	{
	public:
		void ResetRelativeInput();

		void OnKeyAction(int aKey, int aScancode, bool aIsKeyDown, int aMode);
		void OnCursorAction(double aXPosition, double aYPosition);
		void OnScrollAction(double aXOffset, double aYOffset);
		void OnMouseButtonAction(int aButton, int aAction, int aModifier);

		const Math::Vector2f& GetMousePosition() const { return mMousePosition; }
		const Math::Vector2f& GetScrollOffset() const { return mScrollOffset; }
		Math::Vector2f GetMousePositionDelta() const { return mMousePosition - mPreviousMousePosition; }
		bool IsKeyDown(Key aKey) const;
		bool IsMouseButtonDown(MouseButton aMouseButton) const;

	private:
		std::map<Key, bool> myKeys{};
		std::map<MouseButton, bool> myMouseButtons{};
		Math::Vector2f mPreviousMousePosition{};
		Math::Vector2f mMousePosition{};
		Math::Vector2f mScrollOffset{}; // Relative offset per frame
	};
}
