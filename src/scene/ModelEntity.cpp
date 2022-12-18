//
// Created by Seth Hahn on 11/30/22.
//

#include "ModelEntity.h"

namespace bird {

    ModelEntity::ModelEntity(Mesh* pMeshes, uint16_t meshCount) : m_pMeshes(pMeshes), m_meshCount(meshCount) {

    }

    ModelEntity::~ModelEntity() {

    }

    const Mesh* const ModelEntity::getMeshes() const {
        return m_pMeshes;
    }

    const uint16_t& ModelEntity::getMeshCount() const {
        return m_meshCount;
    }

}
