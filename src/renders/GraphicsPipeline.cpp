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