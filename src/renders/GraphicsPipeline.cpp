//
// Created by Seth Hahn on 11/6/22.
//

#include "GraphicsPipeline.h"

#include <pstl/glue_algorithm_defs.h>

namespace bird {

GraphicsPipelineType CURRENT_GRAPHICS_PIPELINE = GRAPHICS_PIPELINE_NONE;

GraphicsPipeline::GraphicsPipeline(GraphicsPipelineType graphicsPipeline) {
	CURRENT_GRAPHICS_PIPELINE = graphicsPipeline;
	m_defaultCamera =
		new DefaultCamera(1280, 720);  // TODO: Make this configurable
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
