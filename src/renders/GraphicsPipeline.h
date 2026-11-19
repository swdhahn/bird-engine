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
#include "../scene/components/MeshComponent.h"
#include "Window.h"
#include "generic/FrameBuffer.h"

namespace bird {

/**
 * Standard uniform buffer layout for camera and view parameters.
 * Matches std140 layout for binding 0 in engine vertex shaders.
 */
struct GlobalUBOData {
	Matrix4 perspective;
	Matrix4 view;
	Vector4 viewPos;
};

/**
 * Push constant block providing per-draw object transformation matrices.
 * Matches Vulkan-style push_constant layout in engine shaders.
 */
struct ObjectPushConstants {
	Matrix4 model;
	/** Inverse-transpose of the model's 3x3, as std430 mat3 (vec3 columns
	 * padded to 16 bytes). Lights normals correctly on rotated objects. */
	Vector4 normalMatrix[3];

	explicit ObjectPushConstants(const Matrix4& modelMatrix) : model(modelMatrix) {
		const Matrix3 normal = glm::transpose(glm::inverse(Matrix3(modelMatrix)));
		for (int column = 0; column < 3; column++) {
			normalMatrix[column] = Vector4(normal[column], 0.0f);
		}
	}
};

/**
 * Standard uniform buffer layout for surface material properties.
 */
struct MaterialUBOData {
	Vector4 color;
	Vector3 specularColor;
	Vector3 ambientColor;
	float shininess;
};

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

	/** Shared full-screen quad mesh used for post-processing passes. */
	static std::shared_ptr<Mesh> s_quad_mesh;

   protected:
	std::unique_ptr<Window> m_pWindow = nullptr;
	Camera* m_camera;
	Matrix4 m_orthoMatrix;
	DefaultCamera* m_defaultCamera;
	std::unique_ptr<Buffer<MaterialUBOData>> m_materialUBO;
	std::unique_ptr<Buffer<GlobalUBOData>> m_globalUBO;

	std::vector<MeshComponent*> m_meshes;
	std::vector<std::unique_ptr<FrameBuffer>> m_framebuffers;
};

}  // namespace bird

#endif	// BIRD_GRAPHICSPIPELINE_H
