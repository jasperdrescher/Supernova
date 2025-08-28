#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

class Camera
{
public:
	enum CameraType { lookat, firstperson };
	CameraType type = CameraType::lookat;

	glm::vec3 mRotation;
	glm::vec3 mPosition;
	glm::vec4 viewPos;

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

	float getNearClip() const;

	float getFarClip() const;

	void setPerspective(float fov, float aspect, float znear, float zfar);

	void updateAspectRatio(float aspect);

	void setPosition(glm::vec3 position);

	void setRotation(glm::vec3 rotation);

	void rotate(glm::vec3 delta);

	void setTranslation(glm::vec3 translation);

	void translate(glm::vec3 delta);

	void setRotationSpeed(float rotationSpeed);

	void setMovementSpeed(float movementSpeed);

	void update(float deltaTime);

private:
	void UpdateViewMatrix();

	float mFoV;
	float mZNear;
	float mZFar;
};
