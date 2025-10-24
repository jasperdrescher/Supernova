#pragma once

#include "Types.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>

namespace Math
{
	inline constexpr Matrix4f Translate(const Matrix4f& aMatrix, const Vector3f& aTranslation)
	{
		return glm::translate(aMatrix, aTranslation);
	}

	inline Matrix4f Rotate(const Matrix4f& aMatrix, const float aAngle, const Vector3f& aAxis)
	{
		return glm::rotate(aMatrix, aAngle, aAxis);
	}

	inline Matrix4f Scale(const Matrix4f& aMatrix, const Vector3f& aScale)
	{
		return glm::scale(aMatrix, aScale);
	}

	inline Matrix4f Perspective(const float aFoV, const float AspectRatio, const float aNear, const float aFar)
	{
		return glm::perspective(aFoV, AspectRatio, aNear, aFar);
	}

	inline constexpr float ToRadians(const float aDegrees)
	{
		return glm::radians(aDegrees);
	}

	inline float* ValuePointer(Vector3f& aVector)
	{
		return glm::value_ptr(aVector);
	}

	inline float* ValuePointer(Vector4f& aVector)
	{
		return glm::value_ptr(aVector);
	}

	inline Vector3f Normalize(const Vector3f& aVector)
	{
		return glm::normalize(aVector);
	}

	inline Vector4f Normalize(const Vector4f& aVector)
	{
		return glm::normalize(aVector);
	}

	inline constexpr Vector3f Cross(const Vector3f& aX, const Vector3f& aY)
	{
		return glm::cross(aX, aY);
	}

	inline bool Decompose(const Matrix4f& aMatrix, Vector3f& aScale, Quaternionf& aOrientation, Vector3f& aTranslation, Vector3f& aSkew, Vector4f& aPerspective)
	{
		return glm::decompose(aMatrix, aScale, aOrientation, aTranslation, aSkew, aPerspective);
	}

	inline bool Decompose(const Matrix4f& aMatrix, Vector3f& aScale, Quaternionf& aOrientation, Vector3f& aTranslation)
	{
		Vector3f skew{0.0f};
		Vector4f perspective{0.0f};
		return glm::decompose(aMatrix, aScale, aOrientation, aTranslation, skew, perspective);
	}

	inline float Sine(const float aAngle)
	{
		return std::sin(aAngle);
	}

	inline float Cosine(const float aAngle)
	{
		return std::cos(aAngle);
	}

	inline float Tangent(const float aAngle)
	{
		return std::tan(aAngle);
	}
};
