//
// Created by Seth Hahn on 10/5/23.
//

#ifndef BIRD_CAMERA_H
#define BIRD_CAMERA_H

#include <memory>

#include "../util/Math.h"
#include "WorldObject.h"

namespace bird {

class EnvironmentMap;

/**
 * Environmental rendering settings associated with a camera view,
 * including viewport clear color, ambient fallback, IBL environment map,
 * skybox toggle, exposure, and G-buffer debug visualization.
 */
class WorldEnvironment {
   public:
	/** Background color in linear HDR space, shown when no skybox is rendered. */
	Vector3 clearColor;
	/** Uniform ambient light color (linear), used when environmentMap is null. */
	Vector3 ambientColor;
	/** Ambient light multiplier, used when environmentMap is null. */
	float ambientIntensity;
	/** Linear exposure multiplier applied before tone mapping. */
	float exposure;
	/**
	 * G-buffer debug visualization: 0 final image, 1 albedo, 2 normals,
	 * 3 roughness/metallic, 4 linear depth, 5 directional shadow atlas,
	 * 6 local shadow atlas.
	 */
	int debugView;
	/**
	 * Optional precomputed IBL environment (skybox + diffuse irradiance +
	 * specular prefilter cubemaps). When non-null, replaces flat ambient
	 * lighting with physically-based IBL in deferred and forward passes.
	 */
	std::shared_ptr<EnvironmentMap> environmentMap;
	/**
	 * Multiplier applied to diffuse + specular IBL contribution (>= 0).
	 * Set to 0.0f to keep the skybox visible while disabling IBL shading.
	 */
	float iblIntensity;
	/**
	 * When true and environmentMap is non-null, renders the environment
	 * cubemap as a background skybox before the forward transparency pass.
	 */
	bool renderSkybox;

	WorldEnvironment()
		: clearColor(0, 0, 0),
		  ambientColor(0.6f, 0.7f, 0.9f),
		  ambientIntensity(0.15f),
		  exposure(1.0f),
		  debugView(0),
		  environmentMap(nullptr),
		  iblIntensity(1.0f),
		  renderSkybox(true) {}
};

/**
 * 3D perspective camera entity.
 *
 * Inherits from WorldObject to allow spatial positioning and rotation within
 * the scene tree. Computes the 3D perspective projection matrix (FOV 70°,
 * near 0.1, far 1000.0) and supplies view matrix data to shaders.
 */
class Camera : public WorldObject {
	BIRD_OBJECT(CAMERA)
   public:
	/**
	 * Creates a perspective camera configured for the given render target dimensions.
	 * @param width Viewport width in pixels (must be non-zero).
	 * @param height Viewport height in pixels (must be non-zero).
	 */
	Camera(uint32_t width, uint32_t height);
	virtual ~Camera();

	/**
	 * Binds this camera as the active rendering camera in the graphics pipeline.
	 */
	void enable();

	const Matrix4& getPerspectiveMatrix() const;

	/**
	 * World-to-view matrix: the inverse of the camera's rigid transform
	 * (position and rotation; scale is ignored). The camera looks down its
	 * local -Z axis.
	 */
	Matrix4 getViewMatrix() const;

	/**
	 * Rebuilds the projection matrix for a new viewport aspect ratio.
	 * @param width Render target width in pixels (must be > 0).
	 * @param height Render target height in pixels (must be > 0).
	 * @throws std::invalid_argument when either dimension is zero.
	 */
	void setViewportSize(uint32_t width, uint32_t height);

	void setWorldEnvironment(WorldEnvironment env);
	WorldEnvironment& getWorldEnvironment();

	/** Vertical field of view in radians. */
	float getFieldOfView() const { return kFieldOfView; }
	float getNearPlane() const { return kNearPlane; }
	float getFarPlane() const { return kFarPlane; }

   protected:
	WorldEnvironment m_env;

   private:
	// Vertical field of view in radians (70 degrees).
	static constexpr float kFieldOfView = 70.0f * 3.14159265358979f / 180.0f;
	static constexpr float kNearPlane = 0.1f;
	static constexpr float kFarPlane = 1000.0f;

	Matrix4 m_perspectiveMatrix;
};
}  // namespace bird

#endif	// BIRD_CAMERA_H
