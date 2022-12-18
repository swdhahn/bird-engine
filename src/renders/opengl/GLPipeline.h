//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_GLPIPELINE_H
#define BIRD_GLPIPELINE_H

#include "../GraphicsPipeline.h"
#include "GLMesh.h"
#include "../../util/File.h"

namespace bird::gl {

    class GLPipeline : public GraphicsPipeline {
    public:
        GLPipeline();
        ~GLPipeline() override;
        void init() override;
        void cleanUp() override;

        void renderScene(const Scene* scene) override;

        std::string getName() const override;

    private:

    };

} // bird

#endif //BIRD_GLPIPELINE_H
