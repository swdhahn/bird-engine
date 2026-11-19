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

   private:
	float m_speed = 10.0f;
};

}  // namespace bird

#endif	// BIRD_DEFAULTCAMERA_H
