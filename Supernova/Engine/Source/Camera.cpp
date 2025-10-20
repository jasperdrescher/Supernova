#include "Camera.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <Profiler/SimpleProfiler.hpp>

Camera::Camera()
	: mMatrices{}
	, mViewPosition{0.0f}
	, mPosition{0.0f}
	, mRotation{0.0f}
	, mType{CameraType::LookAt}
	, mFoV{0.0f}
	, mZNear{0.0f}
	, mZFar{0.0f}
	, mRotationSpeed{1.0f}
	, mMovementSpeed{1.0f}
	, mZoomSpeed{1.0f}
	, mFastMovementSpeedMultiplier{4.0f}
	, mFlipY{false}
{
}

void Camera::UpdateViewMatrix()
{
	const glm::mat4 currentMatrix = mMatrices.mView;

	glm::mat4 rotationMatrix = glm::mat4(1.0f);

	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(mRotation.x * (mFlipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationMatrix = glm::rotate(rotationMatrix, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::vec3 translation = mPosition;
	if (mFlipY)
	{
		translation.y *= -1.0f;
	}

	glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), translation);

	if (mType == CameraType::FirstPerson)
	{
		mMatrices.mView = rotationMatrix * translationMatrix;
	}
	else
	{
		mMatrices.mView = translationMatrix * rotationMatrix;
	}

	mViewPosition = glm::vec4(mPosition, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);
}

void Camera::SetType(CameraType aType)
{
	mType = aType;
}

void Camera::SetPerspective(float aFoV, float aAspectRatio, float aZNear, float aZFar)
{
	const glm::mat4 currentMatrix = mMatrices.mPerspective;
	mFoV = aFoV;
	mZNear = aZNear;
	mZFar = aZFar;
	mMatrices.mPerspective = glm::perspective(glm::radians(aFoV), aAspectRatio, aZNear, aZFar);

	if (mFlipY)
	{
		mMatrices.mPerspective[1][1] *= -1.0f;
	}
};

void Camera::UpdateAspectRatio(float aAspectRatio)
{
	const glm::mat4 currentMatrix = mMatrices.mPerspective;
	mMatrices.mPerspective = glm::perspective(glm::radians(mFoV), aAspectRatio, mZNear, mZFar);

	if (mFlipY)
	{
		mMatrices.mPerspective[1][1] *= -1.0f;
	}
}

void Camera::SetPosition(glm::vec3 aPosition)
{
	mPosition = aPosition;
	UpdateViewMatrix();
}

void Camera::SetRotation(glm::vec3 aRotation)
{
	mRotation = aRotation;
	UpdateViewMatrix();
}

void Camera::Rotate(glm::vec3 aDelta)
{
	mRotation += aDelta;
	UpdateViewMatrix();
}

void Camera::SetTranslation(glm::vec3 aTranslation)
{
	mPosition = aTranslation;
	UpdateViewMatrix();
};

void Camera::Translate(glm::vec3 delta)
{
	mPosition += delta;
	UpdateViewMatrix();
}

void Camera::SetRotationSpeed(float aRotationSpeed)
{
	mRotationSpeed = aRotationSpeed;
}

void Camera::SetMovementSpeed(float aMovementSpeed)
{
	mMovementSpeed = aMovementSpeed;
}

void Camera::SetZoomSpeed(float aZoomSpeed)
{
	mZoomSpeed = aZoomSpeed;
}

void Camera::Update(float aDeltaTime)
{
	SIMPLE_PROFILER_PROFILE_SCOPE("Camera::Update");

	if (mType == CameraType::FirstPerson)
	{
		if (IsMoving())
		{
			glm::vec3 cameraFront;
			cameraFront.x = -std::cos(glm::radians(mRotation.x)) * std::sin(glm::radians(mRotation.y));
			cameraFront.y = std::sin(glm::radians(mRotation.x));
			cameraFront.z = std::cos(glm::radians(mRotation.x)) * std::cos(glm::radians(mRotation.y));
			cameraFront = glm::normalize(cameraFront);

			const float speedMultiplier = mKeys.mIsShiftDown ? mFastMovementSpeedMultiplier : 1.0f;
			const float moveSpeed = (aDeltaTime * mMovementSpeed) * speedMultiplier;

			if (mKeys.mIsUpDown)
				mPosition += cameraFront * moveSpeed;

			if (mKeys.mIsDownDown)
				mPosition -= cameraFront * moveSpeed;

			if (mKeys.mIsLeftDown)
				mPosition -= glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

			if (mKeys.mIsRightDown)
				mPosition += glm::normalize(glm::cross(cameraFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

			if (mKeys.mIsSpaceDown)
				mPosition += glm::normalize(glm::cross(cameraFront, glm::vec3(1.0f, 0.0f, 0.0f))) * moveSpeed;

			if (mKeys.mIsCtrlDown)
				mPosition -= glm::normalize(glm::cross(cameraFront, glm::vec3(1.0f, 0.0f, 0.0f))) * moveSpeed;
		}
	}
	else if (mType == CameraType::LookAt)
	{
		if (mMouse.mScrollWheelDelta != 0.0f)
			Translate(glm::vec3(0.0f, 0.0f, mMouse.mScrollWheelDelta * mZoomSpeed * aDeltaTime));

		if (mMouse.mIsLeftDown)
			Rotate(glm::vec3(mMouse.mDeltaY * mRotationSpeed * aDeltaTime, -mMouse.mDeltaX * mRotationSpeed * aDeltaTime, 0.0f));
		
		if (mMouse.mIsMiddleDown)
			Translate(glm::vec3(-mMouse.mDeltaX * mMovementSpeed * aDeltaTime, -mMouse.mDeltaY * mMovementSpeed * aDeltaTime, 0.0f));
	}

	UpdateViewMatrix();
};

bool Camera::IsMoving() const
{
	return mKeys.mIsLeftDown || mKeys.mIsRightDown || mKeys.mIsUpDown || mKeys.mIsDownDown || mKeys.mIsSpaceDown || mKeys.mIsCtrlDown;
}

float Camera::GetNearClip() const
{
	return mZNear;
}

float Camera::GetFarClip() const
{
	return mZFar;
}
