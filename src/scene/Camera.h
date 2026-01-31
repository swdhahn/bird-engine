//
// Created by Seth Hahn on 10/5/23.
//

#ifndef BIRD_CAMERA_H
#define BIRD_CAMERA_H

#include "../util/Math.h"
#include "Entity.h"

namespace bird {

class Camera : public Entity {
   public:
	Camera(uint32_t width, uint32_t height);
	virtual ~Camera();

	const Matrix4& getPerspectiveMatrix() const;

   private:
	Matrix4 m_perspectiveMatrix;
};
}  // namespace bird

#endif	// BIRD_CAMERA_H
