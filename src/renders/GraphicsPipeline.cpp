//
// Created by Seth Hahn on 11/6/22.
//

#include "GraphicsPipeline.h"

namespace bird {

    GraphicsPipeline::GraphicsPipeline(GraphicsPipelineType graphicsPipeline) {
        s_graphicsPipeline = graphicsPipeline;
    }

    const GraphicsPipelineType GraphicsPipeline::getGraphicsPipelineType() {
        return s_graphicsPipeline;
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