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
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} mKeys;

	void SetType(CameraType aType);

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

	glm::vec4 mViewPosition;
	glm::vec3 mRotation;
	glm::vec3 mPosition;
	CameraType mType;
	float mFoV;
	float mZNear;
	float mZFar;
	float mRotationSpeed;
	float mMovementSpeed;
	bool mIsUpdated;
	bool mFlipY;
};
