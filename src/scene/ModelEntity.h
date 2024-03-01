//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MODELENTITY_H
#define BIRD_MODELENTITY_H

#include "Entity.h"
#include "../renders/generic/Mesh.h"
#include <array>

namespace bird {

    class ModelEntity : public Entity {
    public:
        ModelEntity(std::vector<std::shared_ptr<Mesh>> meshes);
        ModelEntity(std::shared_ptr<Mesh> mesh);
        virtual ~ModelEntity();

        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void process(float delta) = 0;

        const std::vector<std::shared_ptr<Mesh>> getMeshes() const;

    private:
        std::vector<std::shared_ptr<Mesh>> m_meshes;

    };

}

#endif //BIRD_MODELENTITY_H
