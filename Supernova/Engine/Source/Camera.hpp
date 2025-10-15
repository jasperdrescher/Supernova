#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

enum class CameraType { LookAt, FirstPerson };

class Camera
{
public:
	Camera();

	struct Matrices
	{
		Matrices() : mPerspective{0.0f}, mView{0.0f} {}

		glm::mat4 mPerspective;
		glm::mat4 mView;
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
	void SetPosition(glm::vec3 aPosition);
	void SetRotation(glm::vec3 aRotation);
	void Rotate(glm::vec3 aDelta);
	void SetTranslation(glm::vec3 aTranslation);
	void Translate(glm::vec3 aDelta);
	void SetRotationSpeed(float aRotationSpeed);
	void SetMovementSpeed(float aMovementSpeed);
	void SetZoomSpeed(float aZoomSpeed);
	void Update(float aDeltaTime);

	const glm::vec3& GetPosition() const { return mPosition; }
	const glm::vec3& GetRotation() const { return mRotation; }
	const glm::vec4& GetViewPosition() const { return mViewPosition; }
	bool IsMoving() const;
	float GetNearClip() const;
	float GetFarClip() const;

private:
	void UpdateViewMatrix();

	glm::vec4 mViewPosition;
	glm::vec3 mRotation;
	glm::vec3 mPosition;
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
