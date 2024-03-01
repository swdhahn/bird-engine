//
// Created by Seth Hahn on 11/30/22.
//

#include "ModelEntity.h"

namespace bird {

    ModelEntity::ModelEntity(std::vector<std::shared_ptr<Mesh>> meshes) : m_meshes(meshes) {

    }

    ModelEntity::ModelEntity(std::shared_ptr<Mesh> mesh) : m_meshes(std::vector<std::shared_ptr<Mesh>>()) {
        m_meshes.push_back(mesh);
    }

    ModelEntity::~ModelEntity() {

    }

    const std::vector<std::shared_ptr<Mesh>> const ModelEntity::getMeshes() const {
        return m_meshes;
    }

}
