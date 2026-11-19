//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MATH_H
#define BIRD_MATH_H

#include <algorithm>
#define PI 3.141592

#include <stdint.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <iostream>

namespace bird {
class WorldObject;
class ScreenObject;

typedef glm::vec2 Vector2;
typedef glm::vec3 Vector3;
typedef glm::vec4 Vector4;
typedef glm::quat Quaternion;
typedef glm::mat2x2 Matrix2;
typedef glm::mat2x3 Matrix2x3;
typedef glm::mat3x3 Matrix3;
typedef glm::mat3x2 Matrix3x2;
typedef glm::mat3x4 Matrix3x4;
typedef glm::mat4x4 Matrix4;
typedef glm::mat4x2 Matrix4x2;
typedef glm::mat4x3 Matrix4x3;

/**
 * Creates an orientation quaternion from an axis and rotation angle.
 * @param angle Rotation in radians.
 * @param axis Normalized axis of rotation.
 */
inline Quaternion fromAngleAxis(const float& angle, const Vector3& axis) {
	return glm::angleAxis(angle, axis);
}

/**
 * Creates a 4x4 view matrix oriented from eye position looking toward target.
 * @param eyePos Camera eye position in world space.
 * @param lookPos Target focus position in world space.
 * @param upDir World up vector (typically (0, 1, 0)).
 */
inline Matrix4 lookAt(const Vector3& eyePos, const Vector3& lookPos,
					  const Vector3& upDir) {
	return glm::lookAt(eyePos, lookPos, upDir);
}

/**
 * Creates a rotation quaternion pointing along direction.
 * @param direction Normalized forward direction vector.
 * @param upDir Normalized up reference vector.
 */
inline Quaternion lookAt(const Vector3& direction, const Vector3& upDir) {
	return glm::quatLookAt(direction, upDir);
}

/**
 * Computes a combined Translation * Rotation * Scale 4x4 model matrix for an object.
 */
Matrix4 createTransformMatrix(WorldObject* obj);

/**
 * Writes updated 4x4 model matrix directly into destination matrix pointer.
 */
void updateTransformMatrix(WorldObject* obj, Matrix4* dst);

/**
 * Writes updated 3x3 2D affine transform matrix directly into destination.
 */
void updateTransformMatrix(ScreenObject* obj, Matrix3* dst);

/**
 * Tests whether a 2D point lies inside an axis-aligned bounding rectangle.
 * @param point Coordinates of the point to test.
 * @param bounds Rectangular region defined as Vector4(x, y, width, height).
 */
bool contains(const Vector2& point, const Vector4& bounds);

/**
 * Generates a right-handed perspective projection matrix.
 *
 * @param width Viewport width in pixels.
 * @param height Viewport height in pixels.
 * @param fov Field of view in radians.
 * @param near Distance to the near clipping plane (must be > 0).
 * @param far Distance to far clipping plane (-1 creates an infinite projection).
 * @return 4x4 perspective projection matrix.
 */
Matrix4 createPerspectiveMatrix(uint32_t width, uint32_t height, float fov,
								float near, float far);

/**
 * Linearly interpolates between two values, clamping factor to [0, 1].
 */
template <typename T>
T lerp(const T& from, const T& to, float deltaTime) {
	return from + (to - from) * std::clamp(deltaTime, 0.0f, 1.0f);
}

std::ostream& operator<<(std::ostream& out, const Vector2& s);
std::ostream& operator<<(std::ostream& out, const Vector3& s);
std::ostream& operator<<(std::ostream& out, const Vector4& s);
std::ostream& operator<<(std::ostream& out, const Quaternion& s);
std::ostream& operator<<(std::ostream& out, const Matrix2& s);
std::ostream& operator<<(std::ostream& out, const Matrix2x3& s);
std::ostream& operator<<(std::ostream& out, const Matrix3& s);
std::ostream& operator<<(std::ostream& out, const Matrix3x2& s);
std::ostream& operator<<(std::ostream& out, const Matrix3x4& s);
std::ostream& operator<<(std::ostream& out, const Matrix4& s);
std::ostream& operator<<(std::ostream& out, const Matrix4x2& s);
std::ostream& operator<<(std::ostream& out, const Matrix4x3& s);

}  // namespace bird

#endif	// BIRD_MATH_H
