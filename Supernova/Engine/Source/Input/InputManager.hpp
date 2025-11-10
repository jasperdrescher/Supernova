#pragma once

#include "InputKeys.hpp"
#include "Math/Types.hpp"

#include <map>
#include <shared_mutex>

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

		void ResetRelativeInput();

		void OnKeyAction(int aKey, int aScancode, bool aIsKeyDown, int aMode);
		void OnCursorAction(double aXPosition, double aYPosition);
		void OnScrollAction(double aXOffset, double aYOffset);
		void OnMouseButtonAction(int aButton, int aAction, int aModifier);

		const Math::Vector2f& GetMousePosition() const { return mMousePosition; }
		const Math::Vector2f& GetScrollOffset() const { return mScrollOffset; }
		Math::Vector2f GetMousePositionDelta() const { return mMousePosition - mPreviousMousePosition; }
		bool GetIsKeyDown(Key aKey) const;
		bool GetIsMouseButtonDown(MouseButton aMouseButton) const;

	private:
		InputManager() = default;
		~InputManager() = default;

		mutable std::shared_mutex mMutex;
		std::map<Key, bool> myKeys{};
		std::map<MouseButton, bool> myMouseButtons{};
		Math::Vector2f mPreviousMousePosition{};
		Math::Vector2f mMousePosition{};
		Math::Vector2f mScrollOffset{}; // Relative offset per frame
	};
}
