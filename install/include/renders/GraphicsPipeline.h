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

struct GlobalUBOData {
	Matrix4 perspective;
	Matrix4 view;
	Matrix4 model;
	Vector4 viewPos;
};

struct MaterialUBOData {
	Vector3 color;
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

	virtual void renderRootScene(const Scene* scene) = 0;

	virtual std::string getName() const = 0;

	std::unique_ptr<Window>& getWindow();

	void setCamera(Camera* camera);
	Camera* getCamera();

	void addMesh(MeshComponent* mesh);
	void removeMesh(MeshComponent* mesh);

   protected:
	std::unique_ptr<Window> m_pWindow = nullptr;
	Camera* m_camera;
	DefaultCamera* m_defaultCamera;
	std::unique_ptr<Buffer<MaterialUBOData>> m_materialUBO;
	std::unique_ptr<Buffer<GlobalUBOData>> m_globalUBO;

	// std::list fix this
	std::vector<MeshComponent*> m_meshes;
	std::vector<std::unique_ptr<FrameBuffer>> m_framebuffers;
	std::shared_ptr<Mesh> m_quad_mesh;
};

}  // namespace bird

#endif	// BIRD_GRAPHICSPIPELINE_H
