//
// Created by Seth Hahn on 10/5/23.
//

#include "Camera.h"

namespace bird {

    Camera::Camera(uint32_t width, uint32_t height) : Entity() {
        m_perspectiveMatrix = createPerspectiveMatrix(width, height, 70.0f, 0.1f, 1000.0f);

    }

    Camera::~Camera() {

    }

    const Matrix4 &Camera::getPerspectiveMatrix() const {
        return m_perspectiveMatrix;
    }

}
