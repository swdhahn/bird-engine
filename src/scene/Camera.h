//
// Created by Seth Hahn on 10/5/23.
//

#ifndef BIRD_CAMERA_H
#define BIRD_CAMERA_H

#include "../util/Math.h"
#include "WorldObject.h"

namespace bird {

class WorldEnvironment {
   public:
	Vector3 clearColor;

	WorldEnvironment() : clearColor(0, 0, 0) {}
};

class Camera : public WorldObject {
	BIRD_OBJECT(CAMERA)

	Camera(uint32_t width, uint32_t height);
	virtual ~Camera();

	void enable();

	const Matrix4& getPerspectiveMatrix() const;
	/**
	 * Rebuilds the projection for a new aspect ratio.
	 * @param width, height - render target size in pixels; both non-zero.
	 * @throws std::invalid_argument when either dimension is zero.
	 */
	void setViewportSize(uint32_t width, uint32_t height);
	void setWorldEnvironment(WorldEnvironment env);
	WorldEnvironment& getWorldEnvironment();

   protected:
	WorldEnvironment m_env;

   private:
	static constexpr float kFieldOfView = 70.0f;
	static constexpr float kNearPlane = 0.1f;
	static constexpr float kFarPlane = 1000.0f;

	Matrix4 m_perspectiveMatrix;
};
}  // namespace bird

#endif	// BIRD_CAMERA_H
