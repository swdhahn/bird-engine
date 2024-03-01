//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_GRAPHICSPIPELINE_H
#define BIRD_GRAPHICSPIPELINE_H

#include "Window.h"
#include <string>
#include "../scene/Scene.h"
#include "../scene/ModelEntity.h"
#include "../scene/Camera.h"
#include "../scene/DefaultCamera.h"

namespace bird {

    class GraphicsPipeline {
    public:
        GraphicsPipeline(GraphicsPipelineType graphicsPipeline);
        virtual ~GraphicsPipeline();
        virtual void init() = 0;
        virtual void cleanUp() = 0;

        virtual void renderRootScene(const Scene* scene) = 0;

        virtual std::string getName() const = 0;

        static const GraphicsPipelineType getGraphicsPipelineType();
        std::unique_ptr<Window>& getWindow();

        void setCamera(Camera* camera);

    protected:
        std::unique_ptr<Window> m_pWindow = nullptr;
        inline static GraphicsPipelineType s_graphicsPipeline = GRAPHICS_PIPELINE_NONE;
        Camera* m_camera;
        DefaultCamera* m_defaultCamera;

        void getModelEntities(const Scene* const scene, std::vector<ModelEntity*>& entities) const;



    };

}


#endif //BIRD_GRAPHICSPIPELINE_H
