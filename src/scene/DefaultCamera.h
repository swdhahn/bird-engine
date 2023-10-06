//
// Created by student on 10/5/23.
//

#ifndef BIRD_DEFAULTCAMERA_H
#define BIRD_DEFAULTCAMERA_H

#include "Camera.h"

namespace bird {

    class DefaultCamera : public Camera {
    public:
        DefaultCamera(uint32_t width, uint32_t height);
        ~DefaultCamera();

        void init() override;
        void deinit() override;

        void process(float delta) override;
    };

} // bird

#endif //BIRD_DEFAULTCAMERA_H
