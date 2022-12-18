//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MODELENTITY_H
#define BIRD_MODELENTITY_H

#include "Entity.h"
#include "../renders/generic/Mesh.h"

namespace bird {

    class ModelEntity : public Entity {
    public:
        ModelEntity(Mesh* pMeshes, uint16_t meshCount);
        virtual ~ModelEntity();

        virtual void init() = 0;
        virtual void deinit() = 0;
        virtual void process(float delta) = 0;

        const Mesh* const getMeshes() const;
        const uint16_t& getMeshCount() const;

    private:
        Mesh* m_pMeshes;
        uint16_t m_meshCount;

    };

}

#endif //BIRD_MODELENTITY_H
