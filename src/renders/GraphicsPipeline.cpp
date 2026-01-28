//
// Created by Seth Hahn on 11/6/22.
//

#include "GraphicsPipeline.h"


#include <cstring>
#include <memory>

#include "generic/Mesh.h"

namespace bird {

GraphicsPipelineType CURRENT_GRAPHICS_PIPELINE = GRAPHICS_PIPELINE_NONE;
uint32_t WINDOW_WIDTH = 1280;
uint32_t WINDOW_HEIGHT = 720;

GraphicsPipeline::GraphicsPipeline(GraphicsPipelineType graphicsPipeline) {
	CURRENT_GRAPHICS_PIPELINE = graphicsPipeline;
	m_defaultCamera = new DefaultCamera(
		WINDOW_WIDTH, WINDOW_HEIGHT);  // TODO: Make this configurable
	m_camera = m_defaultCamera;
}

GraphicsPipeline::~GraphicsPipeline() {
	delete m_defaultCamera;
}

void GraphicsPipeline::initializeGraphicSpecifics() {
	std::unique_ptr<GlobalUBOData[]> globalUBOData =
		std::make_unique<GlobalUBOData[]>(1);
	m_globalUBO = createBuffer<GlobalUBOData>(
		static_cast<BufferMode>(BUFFER_UNIFORM | BUFFER_STAGED),
		std::move(globalUBOData), 1);
	m_globalUBO->setBindingPoint(0);
	m_globalUBO->initialize();
	std::unique_ptr<MaterialUBOData[]> material =
		std::make_unique<MaterialUBOData[]>(1);
	m_materialUBO = createBuffer<MaterialUBOData>(
		static_cast<BufferMode>(BUFFER_UNIFORM | BUFFER_STAGED),
		std::move(material), 1);
	m_materialUBO->setBindingPoint(1);
	m_materialUBO->initialize();

	const float verticies[] = {
		// Triangle 1
		-1.0f, 1.0f, 0.0f,	 // 0: Top-Left
		-1.0f, -1.0f, 0.0f,	 // 1: Bottom-Left
		1.0f, 1.0f, 0.0f,	 // 2: Top-Right

		// Triangle 2
		1.0f, 1.0f, 0.0f,	 // 3: Top-Right (re-used)
		-1.0f, -1.0f, 0.0f,	 // 4: Bottom-Left (re-used)
		1.0f, -1.0f, 0.0f	 // 5: Bottom-Right
	};
	const float texCoords[] = {
		// Triangle 1
		0.0f, 1.0f,	 // 0: Top-Left
		0.0f, 0.0f,	 // 1: Bottom-Left
		1.0f, 1.0f,	 // 2: Top-Right

		// Triangle 2
		1.0f, 1.0f,	 // 3: Top-Right (re-used)
		0.0f, 0.0f,	 // 4: Bottom-Left (re-used)
		1.0f, 0.0f	 // 5: Bottom-Right
	};
	std::unique_ptr<float[]> vert =
		std::make_unique<float[]>(sizeof(float) * 3 * 6);
	memcpy(vert.get(), verticies, sizeof(float) * 3 * 6);
	std::unique_ptr<float[]> tex =
		std::make_unique<float[]>(sizeof(float) * 2 * 6);
	memcpy(tex.get(), texCoords, sizeof(float) * 2 * 6);

	std::unique_ptr<bird::Buffer<float>> vertexBuffer = createBuffer<float>(
		static_cast<bird::BufferMode>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY),
		std::move(vert), 3 * 6);
	std::unique_ptr<bird::Buffer<float, 2>> texCoordBuffer =
		createBuffer<float, 2>(static_cast<bird::BufferMode>(
								   bird::BUFFER_STAGED | bird::BUFFER_ARRAY),
							   std::move(tex), 2 * 6);
	m_quad_mesh = MeshBuilder()
					  .setVertexBuffer(std::move(vertexBuffer))
					  .setTextureCoordBuffer(std::move(texCoordBuffer))
					  .build();
}

std::unique_ptr<Window>& GraphicsPipeline::getWindow() {
	return m_pWindow;
}

void GraphicsPipeline::setCamera(Camera* camera) {
	m_camera = camera;
}

Camera* GraphicsPipeline::getCamera() {
	return m_camera;
}

void GraphicsPipeline::addMesh(MeshComponent* mesh) {
	m_meshes.emplace_back(mesh);
}

void GraphicsPipeline::removeMesh(MeshComponent* mesh) {
	m_meshes.erase(std::remove(m_meshes.begin(), m_meshes.end(), mesh),
				   m_meshes.end());
}
}  // namespace bird
