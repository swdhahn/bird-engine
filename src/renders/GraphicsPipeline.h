//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_GRAPHICSPIPELINE_H
#define BIRD_GRAPHICSPIPELINE_H

#include <memory>
#include <string>
#include <vector>

#include "../scene/Camera.h"
#include "../scene/DefaultCamera.h"
#include "../scene/Scene.h"
#include "../scene/components/LightComponent.h"
#include "../scene/components/MeshComponent.h"
#include "DebugDraw.h"
#include "Window.h"
#include "generic/Buffer.h"
#include "generic/FrameBuffer.h"
#include "generic/FrustumCuller.h"
#include "generic/GpuLayouts.h"
#include "generic/LightList.h"
#include "generic/RenderQueue.h"

namespace bird {

/**
 * Abstract graphics backend abstraction.
 *
 * Implements rendering orchestration across OpenGL, Vulkan, Metal, and DirectX.
 * Owns the application window, active camera, standard global/material uniform
 * buffers, orthographic UI projection, and active mesh component registries.
 */
class GraphicsPipeline {
   public:
	/**
	 * @param graphicsPipeline Type identifier of this backend implementation.
	 */
	GraphicsPipeline(GraphicsPipelineType graphicsPipeline);
	virtual ~GraphicsPipeline();

	/** Initializes graphic API contexts, windowing, buffers, and pipeline shaders. */
	virtual void init() = 0;

	/** Tears down graphics contexts and frees GPU resources. */
	virtual void cleanUp() = 0;

	/** Prepares common engine buffers and default mesh primitives. */
	void initializeGraphicSpecifics();

	/**
	 * Renders an entire scene hierarchy, executes post-processing, and blits UI.
	 * @param scene Active root scene node to render.
	 */
	virtual void renderRootScene(Scene* scene) = 0;

	/** Returns human-readable backend name (e.g. "OpenGL", "Vulkan"). */
	virtual std::string getName() const = 0;

	std::unique_ptr<Window>& getWindow();

	/**
	 * Adapts every size-dependent resource to a new window size.
	 *
	 * Rebuilds post-process framebuffers and updates orthographic projection.
	 * @param framebufferWidth Render target width in physical pixels.
	 * @param framebufferHeight Render target height in physical pixels.
	 * @param uiWidth Window coordinate width for UI projection.
	 * @param uiHeight Window coordinate height for UI projection.
	 */
	virtual void resize(uint32_t framebufferWidth, uint32_t framebufferHeight,
						uint32_t uiWidth, uint32_t uiHeight);

	void setCamera(Camera* camera);
	Camera* getCamera();

	/** Registers an active mesh component to be drawn during scene rendering. */
	void addMesh(MeshComponent* mesh);

	/** Removes an active mesh component from the drawing list. */
	void removeMesh(MeshComponent* mesh);

	/** Registers an active light to illuminate the scene. */
	void addLight(LightComponent* light);

	/** Removes an active light. */
	void removeLight(LightComponent* light);

	/**
	 * World-space debug lines drawn over this frame's 3D scene. The
	 * Application clears the list at the start of each frame.
	 */
	DebugDrawList& getDebugDraw() { return m_debugDraw; }

	/**
	 * Returns the per-frame frustum culling statistics (culled vs. rendered
	 * counts broken down by deferred, forward opaque, transparent, and
	 * particle emitters) collected during the most recent frame.
	 */
	const CullingStats& getCullingStats() const {
		return m_frustumCuller.getStats();
	}

	/**
	 * Enables or disables frustum culling for meshes and particle emitters.
	 * Disabling culling is useful for debugging bounds or profiling culling
	 * savings.
	 * @param enabled True to cull off-screen renderables (default).
	 */
	void setFrustumCullingEnabled(bool enabled) {
		m_frustumCuller.setEnabled(enabled);
	}

	/** @return True when frustum culling is enabled. */
	bool isFrustumCullingEnabled() const {
		return m_frustumCuller.isEnabled();
	}

	/** Shared full-screen quad mesh used for post-processing passes. */
	static std::shared_ptr<Mesh> s_quad_mesh;

   protected:
	/** Packs camera, screen, and environment constants for this frame. */
	GlobalUBOData buildGlobalData() const;

	/**
	 * Rebuilds m_renderQueue from every registered mesh: culls against the
	 * camera frustum, buckets by material, and sorts. Also gathers every
	 * shadow-casting draw, unculled, into m_shadowCasters.
	 * @param frustum Camera frustum.
	 */
	void buildRenderQueue(const Frustum& frustum);

	/** Captures every registered light's world-space state. */
	std::vector<LightSnapshot> snapshotLights();

	std::unique_ptr<Window> m_pWindow = nullptr;
	Camera* m_camera;
	Matrix4 m_orthoMatrix;
	DefaultCamera* m_defaultCamera;
	std::unique_ptr<Buffer<GlobalUBOData>> m_globalUBO;

	std::vector<MeshComponent*> m_meshes;
	std::vector<LightComponent*> m_lights;
	FrustumCuller m_frustumCuller;
	RenderQueue m_renderQueue;
	std::vector<RenderItem> m_shadowCasters;
	std::vector<std::unique_ptr<FrameBuffer>> m_framebuffers;
	DebugDrawList m_debugDraw;
};

}  // namespace bird

#endif	// BIRD_GRAPHICSPIPELINE_H
