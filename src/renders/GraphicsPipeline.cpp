//
// Created by Seth Hahn on 11/6/22.
//

#include "GraphicsPipeline.h"

#include <cstring>
#include <memory>

#include "generic/Mesh.h"
#include "generic/RawBuffer.h"

namespace bird {

GraphicsPipelineType CURRENT_GRAPHICS_PIPELINE = GRAPHICS_PIPELINE_NONE;

std::shared_ptr<Mesh> GraphicsPipeline::s_quad_mesh = nullptr;

uint32_t WINDOW_WIDTH = 1280;
uint32_t WINDOW_HEIGHT = 720;

GraphicsPipeline::GraphicsPipeline(GraphicsPipelineType graphicsPipeline) {
	CURRENT_GRAPHICS_PIPELINE = graphicsPipeline;
	m_defaultCamera = new DefaultCamera(
		WINDOW_WIDTH, WINDOW_HEIGHT);  // TODO: Make this configurable
	m_camera = m_defaultCamera;
	m_orthoMatrix = glm::ortho(0.0f, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT,
							   0.0f, -1.0f, 1.0f);
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

	const float vertices[] = {
		-1.0f, 1.0f,  0.0f,	 // 0: Top-Left
		-1.0f, -1.0f, 0.0f,	 // 1: Bottom-Left
		1.0f,  1.0f,  0.0f,	 // 2: Top-Right
		1.0f,  -1.0f, 0.0f	 // 3: Bottom-Right
	};

	const float texCoords[] = {
		0.0f, 1.0f,	 // 0: Top-Left
		0.0f, 0.0f,	 // 1: Bottom-Left
		1.0f, 1.0f,	 // 2: Top-Right
		1.0f, 0.0f	 // 3: Bottom-Right
	};

	const unsigned int indices[] = {
		0, 1, 2,  // Triangle 1
		2, 1, 3	  // Triangle 2
	};

	std::unique_ptr<float[]> vert =
		std::make_unique<float[]>(sizeof(float) * 3 * 4);
	memcpy(vert.get(), vertices, sizeof(float) * 3 * 4);
	std::unique_ptr<float[]> tex =
		std::make_unique<float[]>(sizeof(float) * 2 * 4);
	memcpy(tex.get(), texCoords, sizeof(float) * 2 * 4);
	std::unique_ptr<uint32_t[]> ind =
		std::make_unique<uint32_t[]>(sizeof(uint32_t) * 6);
	memcpy(ind.get(), indices, sizeof(uint32_t) * 6);

	std::unique_ptr<bird::Buffer<float>> vertexBuffer = createBuffer<float>(
		static_cast<bird::BufferMode>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY),
		std::move(vert), 3 * 4);
	std::unique_ptr<bird::Buffer<float, 2>> texCoordBuffer =
		createBuffer<float, 2>(static_cast<bird::BufferMode>(
								   bird::BUFFER_STAGED | bird::BUFFER_ARRAY),
							   std::move(tex), 2 * 4);
	std::unique_ptr<bird::Buffer<uint32_t>> indicesBuffer =
		createBuffer<uint32_t>(
			static_cast<bird::BufferMode>(bird::BUFFER_STAGED |
										  bird::BUFFER_ELEMENT_ARRAY),
			std::move(ind), 6);
	s_quad_mesh = MeshBuilder()
					  .setIndexBuffer(std::move(indicesBuffer))
					  .setVertexBuffer(std::move(vertexBuffer))
					  .setTextureCoordBuffer(std::move(texCoordBuffer))
					  .build();
}

std::unique_ptr<Window>& GraphicsPipeline::getWindow() {
	return m_pWindow;
}

void GraphicsPipeline::setCamera(Camera* camera) {
	m_camera = camera;
	// A camera built before a resize must adopt the current aspect ratio.
	m_camera->setViewportSize(WINDOW_WIDTH, WINDOW_HEIGHT);
}

void GraphicsPipeline::resize(uint32_t framebufferWidth,
							  uint32_t framebufferHeight, uint32_t uiWidth,
							  uint32_t uiHeight) {
	WINDOW_WIDTH = framebufferWidth;
	WINDOW_HEIGHT = framebufferHeight;
	m_orthoMatrix = glm::ortho(0.0f, static_cast<float>(uiWidth),
							   static_cast<float>(uiHeight), 0.0f, -1.0f, 1.0f);
	m_defaultCamera->setViewportSize(framebufferWidth, framebufferHeight);
	m_camera->setViewportSize(framebufferWidth, framebufferHeight);
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
