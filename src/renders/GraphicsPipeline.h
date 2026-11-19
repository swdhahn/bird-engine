//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_GRAPHICSPIPELINE_H
#define BIRD_GRAPHICSPIPELINE_H

#include <memory>
#include <string>

#include "../scene/Camera.h"
#include "../scene/DefaultCamera.h"
#include "../scene/Scene.h"
#include "../scene/components/MeshComponent.h"
#include "Window.h"
#include "generic/FrameBuffer.h"

namespace bird {

/** Mirrors the std140 `Global` block (binding 0); written once per frame. */
struct GlobalUBOData {
	Matrix4 perspective;
	Matrix4 view;
	Vector4 viewPos;
};

/** Mirrors the `ObjectConstants` push_constant block; written per draw. */
struct ObjectPushConstants {
	Matrix4 model;
};

struct MaterialUBOData {
	Vector4 color;
	Vector3 specularColor;
	Vector3 ambientColor;
	float shininess;
};

class GraphicsPipeline {
   public:
	GraphicsPipeline(GraphicsPipelineType graphicsPipeline);
	virtual ~GraphicsPipeline();
	virtual void init() = 0;
	virtual void cleanUp() = 0;

	void initializeGraphicSpecifics();

	virtual void renderRootScene(Scene* scene) = 0;

	virtual std::string getName() const = 0;

	std::unique_ptr<Window>& getWindow();

	/**
	 * Adapts every size-dependent resource to a new window size.
	 * @param framebufferWidth, framebufferHeight - render target pixels.
	 * @param uiWidth, uiHeight - window coordinates, for the UI projection.
	 */
	virtual void resize(uint32_t framebufferWidth, uint32_t framebufferHeight,
						uint32_t uiWidth, uint32_t uiHeight);

	void setCamera(Camera* camera);
	Camera* getCamera();

	void addMesh(MeshComponent* mesh);
	void removeMesh(MeshComponent* mesh);

	static std::shared_ptr<Mesh> s_quad_mesh;

   protected:
	std::unique_ptr<Window> m_pWindow = nullptr;
	Camera* m_camera;
	Matrix4 m_orthoMatrix;
	DefaultCamera* m_defaultCamera;
	std::unique_ptr<Buffer<MaterialUBOData>> m_materialUBO;
	std::unique_ptr<Buffer<GlobalUBOData>> m_globalUBO;

	// std::list fix this
	std::vector<MeshComponent*> m_meshes;
	std::vector<std::unique_ptr<FrameBuffer>> m_framebuffers;
};

}  // namespace bird

#endif	// BIRD_GRAPHICSPIPELINE_H
