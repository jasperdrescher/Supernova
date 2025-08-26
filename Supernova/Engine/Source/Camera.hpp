#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

class Camera
{
private:
	float fov;
	float znear, zfar;

	void updateViewMatrix();

public:
	enum CameraType { lookat, firstperson };
	CameraType type = CameraType::lookat;

	glm::vec3 rotation;
	glm::vec3 position;
	glm::vec4 viewPos;

	float rotationSpeed = 1.0f;
	float movementSpeed = 1.0f;

	bool updated = true;
	bool flipY = false;

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

	bool moving() const;

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

	// Update camera passing separate axis data (gamepad)
	// Returns true if view or position has been changed
	bool updatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime);
};
