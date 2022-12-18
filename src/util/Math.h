//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MATH_H
#define BIRD_MATH_H

#define PI 3.141592

#include <iostream>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"


namespace bird {
    class WorldObject;

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

    inline Quaternion fromAngleAxis(const float& angle, const Vector3& axis) {
        return glm::angleAxis(angle, axis);
    }

    inline Matrix4 lookAt(const Vector3& eyePos, const Vector3& lookPos, const Vector3& upDir) {

        return glm::lookAt(eyePos, lookPos, upDir);
    }

    inline Quaternion lookAt(const Vector3& direction, const Vector3& upDir) {
        return glm::quatLookAt(direction, upDir);
    }

    Matrix4 createTransformMatrix(WorldObject* obj);
    void updateTransformMatrix(WorldObject* obj, Matrix4* dst);

    /**
     *
     * @param width - width of window
     * @param height - height of window
     * @param fov - fov in radians
     * @param near - near distance plane
     * @param far - far distance plane (value of -1 will create infinite far plane)
     * @return Perspective matrix generated from this method
     */
    Matrix4 createPerspectiveMatrix(uint32_t width, uint32_t height, float fov, float near, float far);

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

}

#endif //BIRD_MATH_H
