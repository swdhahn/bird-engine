//
// Created by Seth Hahn on 11/30/22.
//

#include "Math.h"

#include "../scene/2D/ScreenObject.h"
#include "../scene/WorldObject.h"

namespace bird {

Matrix4 createTransformMatrix(WorldObject* obj) {
	Matrix4 mat = glm::scale(glm::mat4(1), obj->getScale());
	mat = mat * glm::toMat4(obj->getWorldRotation() * obj->getLocalRotation());
	mat =
		glm::translate(mat, obj->getWorldPosition() + obj->getLocalPosition());
	return mat;
}

void updateTransformMatrix(WorldObject* obj, Matrix4* dst) {
	*dst = glm::scale(glm::mat4(1), obj->getScale());
	*dst =
		*dst * glm::toMat4(obj->getWorldRotation() * obj->getLocalRotation());
	*dst =
		glm::translate(*dst, obj->getWorldPosition() + obj->getLocalPosition());
}

void updateTransformMatrix(ScreenObject* obj, Matrix3* dst) {
	float c = cos(obj->getRotation());
	float s = sin(obj->getRotation());

	// Construct the mat3 directly (Column-Major Order)
	// [ Scale.x * cos   -Scale.y * sin    Pos.x ]
	// [ Scale.x * sin    Scale.y * cos    Pos.y ]
	// [      0                0             1   ]

	*dst = glm::mat3(obj->getScale().x * c, obj->getScale().x * s, 0.0f,
					 -(obj->getScale().y * s), obj->getScale().y * c, 0.0f,
					 obj->getPosition().x, obj->getPosition().y, 1.0f);
}
Matrix4 createPerspectiveMatrix(uint32_t width, uint32_t height, float fov,
								float near, float far) {
	return glm::perspective(fov, (float)width / (float)height, near, far);
}

bool contains(const Vector2& point, const Vector4& bounds) {
	if (point.x > bounds.x && point.x < bounds.x + bounds.z &&
		point.y > bounds.y && point.y < bounds.y + bounds.w) {
		return true;
	}
	return false;
}

std::ostream& operator<<(std::ostream& out, const Vector2& s) {
	out << "{" << s.x << ", " << s.y << "}";
	return out;
}

std::ostream& operator<<(std::ostream& out, const Vector3& s) {
	out << "{" << s.x << ", " << s.y << ", " << s.z << "}";
	return out;
}

std::ostream& operator<<(std::ostream& out, const Vector4& s) {
	out << "{" << s.x << ", " << s.y << ", " << s.z << ", " << s.w << "}";
	return out;
}

std::ostream& operator<<(std::ostream& out, const Quaternion& s) {
	out << "{" << s.x << ", " << s.y << ", " << s.z << ", " << s.w << "}";
	return out;
}

std::ostream& operator<<(std::ostream& out, const Matrix2& s) {
	out << "[" << s[0][0] << ", " << s[1][0] << "]\n";
	out << "[" << s[0][1] << ", " << s[1][1] << "]";

	return out;
}

std::ostream& operator<<(std::ostream& out, const Matrix2x3& s) {
	out << "[" << s[0][0] << ", " << s[1][0] << "]\n";
	out << "[" << s[0][1] << ", " << s[1][1] << "]\n";
	out << "[" << s[0][2] << ", " << s[1][2] << "]";
	return out;
}

std::ostream& operator<<(std::ostream& out, const Matrix3& s) {
	out << "[" << s[0][0] << ", " << s[1][0] << ", " << s[2][0] << "]\n";
	out << "[" << s[0][1] << ", " << s[1][1] << ", " << s[2][1] << "]\n";
	out << "[" << s[0][2] << ", " << s[1][2] << ", " << s[2][2] << "]";
	return out;
}

std::ostream& operator<<(std::ostream& out, const Matrix3x2& s) {
	out << "[" << s[0][0] << ", " << s[1][0] << ", " << s[2][0] << "]\n";
	out << "[" << s[0][1] << ", " << s[1][1] << ", " << s[2][1] << "]";
	return out;
}

std::ostream& operator<<(std::ostream& out, const Matrix3x4& s) {
	out << "[" << s[0][0] << ", " << s[1][0] << ", " << s[2][0] << "]\n";
	out << "[" << s[0][1] << ", " << s[1][1] << ", " << s[2][1] << "]\n";
	out << "[" << s[0][2] << ", " << s[1][2] << ", " << s[2][2] << "]\n";
	out << "[" << s[0][3] << ", " << s[1][3] << ", " << s[2][3] << "]";

	return out;
}

std::ostream& operator<<(std::ostream& out, const Matrix4& s) {
	out << "[" << s[0][0] << ", " << s[1][0] << ", " << s[2][0] << ", "
		<< s[3][0] << "]\n";
	out << "[" << s[0][1] << ", " << s[1][1] << ", " << s[2][1] << ", "
		<< s[3][1] << "]\n";
	out << "[" << s[0][2] << ", " << s[1][2] << ", " << s[2][2] << ", "
		<< s[3][2] << "]\n";
	out << "[" << s[0][3] << ", " << s[1][3] << ", " << s[2][3] << ", "
		<< s[3][3] << "]";
	return out;
}

std::ostream& operator<<(std::ostream& out, const Matrix4x2& s) {
	out << "[" << s[0][0] << ", " << s[1][0] << ", " << s[2][0] << ", "
		<< s[3][0] << "]\n";
	out << "[" << s[0][1] << ", " << s[1][1] << ", " << s[2][1] << ", "
		<< s[3][1] << "]";
	return out;
}

std::ostream& operator<<(std::ostream& out, const Matrix4x3& s) {
	out << "[" << s[0][0] << ", " << s[1][0] << ", " << s[2][0] << ", "
		<< s[3][0] << "]\n";
	out << "[" << s[0][1] << ", " << s[1][1] << ", " << s[2][1] << ", "
		<< s[3][1] << "]\n";
	out << "[" << s[0][2] << ", " << s[1][2] << ", " << s[2][2] << ", "
		<< s[3][2] << "]";
	return out;
}

}  // namespace bird
