#pragma once

#include "Math/Types.hpp"

enum class CameraType { LookAt, FirstPerson };

class Camera
{
public:
	Camera();

	struct Matrices
	{
		Matrices() : mPerspective{0.0f}, mView{0.0f} {}

		Math::Matrix4f mPerspective;
		Math::Matrix4f mView;
	} mMatrices;

	struct Keys
	{
		Keys() : mIsLeftDown{false}, mIsRightDown{false}, mIsUpDown{false}, mIsDownDown{false}, mIsShiftDown{false}, mIsSpaceDown{false}, mIsCtrlDown{false} {}

		bool mIsLeftDown;
		bool mIsRightDown;
		bool mIsUpDown;
		bool mIsDownDown;
		bool mIsShiftDown;
		bool mIsSpaceDown;
		bool mIsCtrlDown;
	} mKeys;

	struct Mouse
	{
		Mouse() : mScrollWheelDelta{0.0f}, mDeltaX{0.0f}, mDeltaY{0.0f}, mIsLeftDown { false }, mIsMiddleDown{false} {}

		float mScrollWheelDelta;
		float mDeltaX;
		float mDeltaY;
		bool mIsLeftDown;
		bool mIsMiddleDown;
	} mMouse;

	void SetType(CameraType aType);
	void SetPerspective(float aFoV, float aAspectRatio, float aZNear, float aZFar);
	void UpdateAspectRatio(float aAspectRatio);
	void SetPosition(const Math::Vector3f& aPosition);
	void SetRotation(const Math::Vector3f& aRotation);
	void Rotate(const Math::Vector3f& aDelta);
	void SetTranslation(const Math::Vector3f& aTranslation);
	void Translate(const Math::Vector3f& aDelta);
	void SetRotationSpeed(float aRotationSpeed);
	void SetMovementSpeed(float aMovementSpeed);
	void SetZoomSpeed(float aZoomSpeed);
	void Update(float aDeltaTime);

	const Math::Vector3f& GetPosition() const { return mPosition; }
	const Math::Vector3f& GetRotation() const { return mRotation; }
	const Math::Vector4f& GetViewPosition() const { return mViewPosition; }
	bool IsMoving() const;
	float GetNearClip() const;
	float GetFarClip() const;

private:
	void UpdateViewMatrix();

	Math::Vector4f mViewPosition;
	Math::Vector3f mRotation;
	Math::Vector3f mPosition;
	CameraType mType;
	float mFoV;
	float mZNear;
	float mZFar;
	float mRotationSpeed;
	float mMovementSpeed;
	float mZoomSpeed;
	float mFastMovementSpeedMultiplier;
	bool mFlipY;
};
