#include "MeshComponent.h"

#include <stdexcept>

#include "../../Application.h"
#include "../../renders/generic/Material.h"
#include "../../renders/generic/Mesh.h"
#include "Component.h"

namespace bird {

MeshComponent::MeshComponent(std::string path)
	: Component(), m_meshes(RESOURCE_MANAGER->loadMesh(path)) {}

MeshComponent::MeshComponent(std::vector<_ref<Mesh>> meshes)
	: Component(), m_meshes(meshes) {}

MeshComponent::MeshComponent(_ref<Mesh> meshes)
	: Component(), m_meshes({meshes}) {}

void MeshComponent::init() {
	if (APPLICATION && APPLICATION->getGraphicsPipeline()) {
		APPLICATION->getGraphicsPipeline()->addMesh(this);
	}
}

void MeshComponent::deinit() {
	if (APPLICATION && APPLICATION->getGraphicsPipeline()) {
		APPLICATION->getGraphicsPipeline()->removeMesh(this);
	}
}

void MeshComponent::process(float delta) {}

std::vector<_ref<Mesh>>& MeshComponent::getMeshes() {
	return m_meshes;
}

int MeshComponent::getMeshCount() {
	return static_cast<int>(m_meshes.size());
}

_ref<Material> MeshComponent::getMaterial(int meshIndex) {
	const size_t index = static_cast<size_t>(meshIndex);
	const _ref<Mesh>& mesh = m_meshes.at(index);
	if (index < m_materialOverrides.size() && m_materialOverrides[index]) {
		return m_materialOverrides[index];
	}
	return mesh->getMaterial();
}

void MeshComponent::setMaterial(int meshIndex, _ref<Material> material) {
	const size_t index = static_cast<size_t>(meshIndex);
	if (index >= m_meshes.size()) {
		throw std::out_of_range("MeshComponent::setMaterial: bad mesh index.");
	}
	m_materialOverrides.resize(m_meshes.size());
	m_materialOverrides[index] = std::move(material);
}

void MeshComponent::setAllMaterials(_ref<Material> material) {
	m_materialOverrides.assign(m_meshes.size(), material);
}

}  // namespace bird
