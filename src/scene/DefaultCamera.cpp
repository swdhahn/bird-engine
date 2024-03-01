//
// Created by student on 10/5/23.
//

#include "DefaultCamera.h"

namespace bird {

    DefaultCamera::DefaultCamera(uint32_t width, uint32_t height) : Camera(width, height) {

    }

    DefaultCamera::~DefaultCamera() {

    }

    void DefaultCamera::init() {

    }

    void DefaultCamera::deinit() {

    }

    void DefaultCamera::process(float delta) {
        if(INPUT->getActionValue("look_up") > 0.5f) {
            //rotateLocal(Vector3(1, 0, 0), delta);
        }
        Vector3 direction = Vector3(INPUT->getActionValue("move_right") - INPUT->getActionValue("move_left"), 0, INPUT->getActionValue("move_backward") - INPUT->getActionValue("move_forward"));

        translate(direction * m_speed * delta);
    }

} // bird