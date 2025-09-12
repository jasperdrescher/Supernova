#include "Camera.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

void Camera::UpdateViewMatrix()
{
	glm::mat4 currentMatrix = matrices.view;

	glm::mat4 rotM = glm::mat4(1.0f);
	glm::mat4 transM;

	rotM = glm::rotate(rotM, glm::radians(mRotation.x * (mFlipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::vec3 translation = mPosition;
	if (mFlipY)
	{
		translation.y *= -1.0f;
	}
	transM = glm::translate(glm::mat4(1.0f), translation);

	if (mType == CameraType::FirstPerson)
	{
		matrices.view = rotM * transM;
	}
	else
	{
		matrices.view = transM * rotM;
	}

	mViewPosition = glm::vec4(mPosition, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

	if (matrices.view != currentMatrix)
	{
		mIsUpdated = true;
	}
}

bool Camera::IsMoving() const
{
	return keys.left || keys.right || keys.up || keys.down;
}

float Camera::GetNearClip() const
{
	return mZNear;
}

float Camera::GetFarClip() const
{
	return mZFar;
}

void Camera::SetPerspective(float aFoV, float aAspectRatio, float aZNear, float aZFar)
{
	const glm::mat4 currentMatrix = matrices.perspective;
	mFoV = aFoV;
	mZNear = aZNear;
	mZFar = aZFar;
	matrices.perspective = glm::perspective(glm::radians(aFoV), aAspectRatio, aZNear, aZFar);

	if (mFlipY)
	{
		matrices.perspective[1][1] *= -1.0f;
	}

	if (matrices.view != currentMatrix)
	{
		mIsUpdated = true;
	}
};

void Camera::UpdateAspectRatio(float aAspectRatio)
{
	const glm::mat4 currentMatrix = matrices.perspective;
	matrices.perspective = glm::perspective(glm::radians(mFoV), aAspectRatio, mZNear, mZFar);
	if (mFlipY)
	{
		matrices.perspective[1][1] *= -1.0f;
	}
	if (matrices.view != currentMatrix)
	{
		mIsUpdated = true;
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

void Camera::Rotate(glm::vec3 delta)
{
	mRotation += delta;
	UpdateViewMatrix();
}

void Camera::SetTranslation(glm::vec3 translation)
{
	mPosition = translation;
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

void Camera::Update(float aDeltaTime)
{
	mIsUpdated = false;
	if (mType == CameraType::FirstPerson)
	{
		if (IsMoving())
		{
			glm::vec3 camFront;
			camFront.x = -std::cos(glm::radians(mRotation.x)) * std::sin(glm::radians(mRotation.y));
			camFront.y = std::sin(glm::radians(mRotation.x));
			camFront.z = std::cos(glm::radians(mRotation.x)) * std::cos(glm::radians(mRotation.y));
			camFront = glm::normalize(camFront);

			const float moveSpeed = aDeltaTime * mMovementSpeed;

			if (keys.up)
				mPosition += camFront * moveSpeed;
			if (keys.down)
				mPosition -= camFront * moveSpeed;
			if (keys.left)
				mPosition -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
			if (keys.right)
				mPosition += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
		}
	}

	UpdateViewMatrix();
};
