#include "MeshComponent.h"

#include "../../Application.h"
#include "Component.h"

namespace bird {

MeshComponent::MeshComponent(std::string path)
	: Component(), m_meshes(RESOURCE_MANAGER->loadMesh(path)) {}

MeshComponent::MeshComponent(std::vector<std::shared_ptr<Mesh>> meshes)
	: Component(), m_meshes(meshes) {}

void MeshComponent::init() {
	APPLICATION->getGraphicsPipeline()->addMesh(this);
}

void MeshComponent::deinit() {
	APPLICATION->getGraphicsPipeline()->removeMesh(this);
}

void MeshComponent::process(float delta) {}

std::vector<std::shared_ptr<Mesh>>& MeshComponent::getMeshes() {
	return m_meshes;
}

}  // namespace bird
