#include "Camera.hpp"

#include "Math/Functions.hpp"
#include "Math/Types.hpp"
#include "Profiler/SimpleProfiler.hpp"

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
	const Math::Matrix4f currentMatrix = mMatrices.mView;

	Math::Matrix4f rotationMatrix = Math::Matrix4f(1.0f);

	rotationMatrix = Math::Rotate(rotationMatrix, Math::ToRadians(mRotation.x * (mFlipY ? -1.0f : 1.0f)), Math::Vector3f(1.0f, 0.0f, 0.0f));
	rotationMatrix = Math::Rotate(rotationMatrix, Math::ToRadians(mRotation.y), Math::Vector3f(0.0f, 1.0f, 0.0f));
	rotationMatrix = Math::Rotate(rotationMatrix, Math::ToRadians(mRotation.z), Math::Vector3f(0.0f, 0.0f, 1.0f));

	Math::Vector3f translation = mPosition;
	if (mFlipY)
	{
		translation.y *= -1.0f;
	}

	const Math::Matrix4f translationMatrix = Math::Translate(Math::Matrix4f(1.0f), translation);
	if (mType == CameraType::FirstPerson)
	{
		mMatrices.mView = rotationMatrix * translationMatrix;
	}
	else
	{
		mMatrices.mView = translationMatrix * rotationMatrix;
	}

	mViewPosition = Math::Vector4f(mPosition, 0.0f) * Math::Vector4f(-1.0f, 1.0f, -1.0f, 1.0f);
}

void Camera::SetType(CameraType aType)
{
	mType = aType;
}

void Camera::SetPerspective(float aFoV, float aAspectRatio, float aZNear, float aZFar)
{
	const Math::Matrix4f currentMatrix = mMatrices.mPerspective;
	mFoV = aFoV;
	mZNear = aZNear;
	mZFar = aZFar;
	mMatrices.mPerspective = Math::Perspective(Math::ToRadians(aFoV), aAspectRatio, aZNear, aZFar);

	if (mFlipY)
	{
		mMatrices.mPerspective[1][1] *= -1.0f;
	}
};

void Camera::UpdateAspectRatio(float aAspectRatio)
{
	const Math::Matrix4f currentMatrix = mMatrices.mPerspective;
	mMatrices.mPerspective = Math::Perspective(Math::ToRadians(mFoV), aAspectRatio, mZNear, mZFar);

	if (mFlipY)
	{
		mMatrices.mPerspective[1][1] *= -1.0f;
	}
}

void Camera::SetPosition(const Math::Vector3f& aPosition)
{
	mPosition = aPosition;
	UpdateViewMatrix();
}

void Camera::SetRotation(const Math::Vector3f& aRotation)
{
	mRotation = aRotation;
	UpdateViewMatrix();
}

void Camera::Rotate(const Math::Vector3f& aDelta)
{
	mRotation += aDelta;
	UpdateViewMatrix();
}

void Camera::SetTranslation(const Math::Vector3f& aTranslation)
{
	mPosition = aTranslation;
	UpdateViewMatrix();
};

void Camera::Translate(const Math::Vector3f& delta)
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
			Math::Vector3f cameraFront{0.0f};
			cameraFront.x = -Math::Cosine(Math::ToRadians(mRotation.x)) * Math::Sine(Math::ToRadians(mRotation.y));
			cameraFront.y = Math::Sine(Math::ToRadians(mRotation.x));
			cameraFront.z = Math::Cosine(Math::ToRadians(mRotation.x)) * Math::Cosine(Math::ToRadians(mRotation.y));
			cameraFront = Math::Normalize(cameraFront);

			const float speedMultiplier = mKeys.mIsShiftDown ? mFastMovementSpeedMultiplier : 1.0f;
			const float moveSpeed = (aDeltaTime * mMovementSpeed) * speedMultiplier;

			if (mKeys.mIsUpDown)
				mPosition += cameraFront * moveSpeed;

			if (mKeys.mIsDownDown)
				mPosition -= cameraFront * moveSpeed;

			if (mKeys.mIsLeftDown)
				mPosition -= Math::Normalize(Math::Cross(cameraFront, Math::Vector3f(0.0f, 1.0f, 0.0f))) * moveSpeed;

			if (mKeys.mIsRightDown)
				mPosition += Math::Normalize(Math::Cross(cameraFront, Math::Vector3f(0.0f, 1.0f, 0.0f))) * moveSpeed;

			if (mKeys.mIsSpaceDown)
				mPosition += Math::Normalize(Math::Cross(cameraFront, Math::Vector3f(1.0f, 0.0f, 0.0f))) * moveSpeed;

			if (mKeys.mIsCtrlDown)
				mPosition -= Math::Normalize(Math::Cross(cameraFront, Math::Vector3f(1.0f, 0.0f, 0.0f))) * moveSpeed;
		}
	}
	else if (mType == CameraType::LookAt)
	{
		if (mMouse.mScrollWheelDelta != 0.0f)
			Translate(Math::Vector3f(0.0f, 0.0f, mMouse.mScrollWheelDelta * mZoomSpeed * aDeltaTime));

		if (mMouse.mIsLeftDown)
			Rotate(Math::Vector3f(mMouse.mDeltaY * mRotationSpeed * aDeltaTime, -mMouse.mDeltaX * mRotationSpeed * aDeltaTime, 0.0f));
		
		if (mMouse.mIsMiddleDown)
			Translate(Math::Vector3f(-mMouse.mDeltaX * mMovementSpeed * aDeltaTime, -mMouse.mDeltaY * mMovementSpeed * aDeltaTime, 0.0f));
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
