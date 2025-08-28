#include "Camera.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

void Camera::updateViewMatrix()
{
	glm::mat4 currentMatrix = matrices.view;

	glm::mat4 rotM = glm::mat4(1.0f);
	glm::mat4 transM;

	rotM = glm::rotate(rotM, glm::radians(mRotation.x * (flipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::vec3 translation = mPosition;
	if (flipY)
	{
		translation.y *= -1.0f;
	}
	transM = glm::translate(glm::mat4(1.0f), translation);

	if (type == CameraType::firstperson)
	{
		matrices.view = rotM * transM;
	}
	else
	{
		matrices.view = transM * rotM;
	}

	viewPos = glm::vec4(mPosition, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);

	if (matrices.view != currentMatrix)
	{
		updated = true;
	}
}

bool Camera::moving() const
{
	return keys.left || keys.right || keys.up || keys.down;
}

float Camera::getNearClip() const
{
	return mZNear;
}

float Camera::getFarClip() const
{
	return mZFar;
}

void Camera::setPerspective(float aFoV, float aAspectRatio, float aZNear, float aZFar)
{
	glm::mat4 currentMatrix = matrices.perspective;
	mFoV = aFoV;
	mZNear = aZNear;
	mZFar = aZFar;
	matrices.perspective = glm::perspective(glm::radians(aFoV), aAspectRatio, aZNear, aZFar);
	if (flipY)
	{
		matrices.perspective[1][1] *= -1.0f;
	}
	if (matrices.view != currentMatrix)
	{
		updated = true;
	}
};

void Camera::updateAspectRatio(float aspect)
{
	glm::mat4 currentMatrix = matrices.perspective;
	matrices.perspective = glm::perspective(glm::radians(mFoV), aspect, mZNear, mZFar);
	if (flipY)
	{
		matrices.perspective[1][1] *= -1.0f;
	}
	if (matrices.view != currentMatrix)
	{
		updated = true;
	}
}

void Camera::setPosition(glm::vec3 aPosition)
{
	mPosition = aPosition;
	updateViewMatrix();
}

void Camera::setRotation(glm::vec3 aRotation)
{
	mRotation = aRotation;
	updateViewMatrix();
}

void Camera::rotate(glm::vec3 delta)
{
	mRotation += delta;
	updateViewMatrix();
}

void Camera::setTranslation(glm::vec3 translation)
{
	mPosition = translation;
	updateViewMatrix();
};

void Camera::translate(glm::vec3 delta)
{
	mPosition += delta;
	updateViewMatrix();
}

void Camera::setRotationSpeed(float aRotationSpeed)
{
	mRotationSpeed = aRotationSpeed;
}

void Camera::setMovementSpeed(float aMovementSpeed)
{
	mMovementSpeed = aMovementSpeed;
}

void Camera::update(float deltaTime)
{
	updated = false;
	if (type == CameraType::firstperson)
	{
		if (moving())
		{
			glm::vec3 camFront;
			camFront.x = -cos(glm::radians(mRotation.x)) * sin(glm::radians(mRotation.y));
			camFront.y = sin(glm::radians(mRotation.x));
			camFront.z = cos(glm::radians(mRotation.x)) * cos(glm::radians(mRotation.y));
			camFront = glm::normalize(camFront);

			float moveSpeed = deltaTime * mMovementSpeed;

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
	updateViewMatrix();
};

// Update camera passing separate axis data (gamepad)
// Returns true if view or position has been changed
bool Camera::updatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime)
{
	bool retVal = false;

	if (type == CameraType::firstperson)
	{
		// Use the common console thumbstick layout		
		// Left = view, right = move

		const float deadZone = 0.0015f;
		const float range = 1.0f - deadZone;

		glm::vec3 camFront;
		camFront.x = -cos(glm::radians(mRotation.x)) * sin(glm::radians(mRotation.y));
		camFront.y = sin(glm::radians(mRotation.x));
		camFront.z = cos(glm::radians(mRotation.x)) * cos(glm::radians(mRotation.y));
		camFront = glm::normalize(camFront);

		float moveSpeed = deltaTime * mMovementSpeed * 2.0f;
		float rotSpeed = deltaTime * mRotationSpeed * 50.0f;

		// Move
		if (fabsf(axisLeft.y) > deadZone)
		{
			float pos = (fabsf(axisLeft.y) - deadZone) / range;
			mPosition -= camFront * pos * ((axisLeft.y < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
			retVal = true;
		}
		if (fabsf(axisLeft.x) > deadZone)
		{
			float pos = (fabsf(axisLeft.x) - deadZone) / range;
			mPosition += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * pos * ((axisLeft.x < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
			retVal = true;
		}

		// Rotate
		if (fabsf(axisRight.x) > deadZone)
		{
			float pos = (fabsf(axisRight.x) - deadZone) / range;
			mRotation.y += pos * ((axisRight.x < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
			retVal = true;
		}
		if (fabsf(axisRight.y) > deadZone)
		{
			float pos = (fabsf(axisRight.y) - deadZone) / range;
			mRotation.x -= pos * ((axisRight.y < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
			retVal = true;
		}
	}
	else
	{
		// todo: move code from example base class for look-at
	}

	if (retVal)
	{
		updateViewMatrix();
	}

	return retVal;
}