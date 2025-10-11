//
// Created by Seth Hahn on 11/6/22.
//

#include "GraphicsPipeline.h"

namespace bird {

    GraphicsPipelineType CURRENT_GRAPHICS_PIPELINE = GRAPHICS_PIPELINE_NONE;

    GraphicsPipeline::GraphicsPipeline(GraphicsPipelineType graphicsPipeline) {
        CURRENT_GRAPHICS_PIPELINE = graphicsPipeline;
        m_defaultCamera = new DefaultCamera(1280, 720); // TODO: Make this configurable
        m_camera = m_defaultCamera;

    }

    GraphicsPipeline::~GraphicsPipeline() {
        delete m_defaultCamera;
    }

    void GraphicsPipeline::initializeGraphicSpecifics() {
        std::unique_ptr<GlobalUBOData[]> globalUBOData = std::make_unique<GlobalUBOData[]>(1);
        m_globalUBO = createBuffer<GlobalUBOData>(static_cast<BufferMode>(BUFFER_UNIFORM | BUFFER_STAGED), std::move(globalUBOData), 1);
        m_globalUBO->setBindingPoint(0);
        m_globalUBO->initialize();
        std::unique_ptr<MaterialUBOData[]> material = std::make_unique<MaterialUBOData[]>(1);
        m_materialUBO = createBuffer<MaterialUBOData>(static_cast<BufferMode>(BUFFER_UNIFORM | BUFFER_STAGED), std::move(material), 1);
        m_materialUBO->setBindingPoint(1);
        m_materialUBO->initialize();
    }

    std::unique_ptr<Window>& GraphicsPipeline::getWindow() {
        return m_pWindow;
    }

    void GraphicsPipeline::setCamera(Camera* camera) {
        m_camera = camera;
    }

    void GraphicsPipeline::getModelEntities(const Scene* const scene, std::vector<ModelEntity*>& entities) const {
        ModelEntity* e = nullptr;
        for(int i = 0; i < scene->getEntities().size(); i++) {
            e = dynamic_cast<ModelEntity*>(scene->getEntities()[i]);
            if(e != nullptr)
                entities.push_back(e);
        }
        for(int i = 0; i < scene->getChildren().size(); i++) {
            getModelEntities(scene->getChildren()[i], entities);
        }
    }


}