#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

class Camera
{
public:
	enum CameraType { LookAt, FirstPerson };
	CameraType mType = CameraType::LookAt;

	glm::vec3 mRotation;
	glm::vec3 mPosition;
	glm::vec4 mViewPosition;

	float mRotationSpeed = 1.0f;
	float mMovementSpeed = 1.0f;

	bool mIsUpdated = true;
	bool mFlipY = false;

	struct
	{
		glm::mat4 perspective;
		glm::mat4 view;
	} matrices;

	struct
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} keys;

	bool IsMoving() const;

	float GetNearClip() const;

	float GetFarClip() const;

	void SetPerspective(float fov, float aspect, float znear, float zfar);

	void UpdateAspectRatio(float aspect);

	void SetPosition(glm::vec3 position);

	void SetRotation(glm::vec3 rotation);

	void Rotate(glm::vec3 delta);

	void SetTranslation(glm::vec3 translation);

	void Translate(glm::vec3 delta);

	void SetRotationSpeed(float rotationSpeed);

	void SetMovementSpeed(float movementSpeed);

	void Update(float aDeltaTime);

private:
	void UpdateViewMatrix();

	float mFoV;
	float mZNear;
	float mZFar;
};
