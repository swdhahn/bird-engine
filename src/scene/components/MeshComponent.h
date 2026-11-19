#ifndef H_BIRD_MESH_COMPONENT
#define H_BIRD_MESH_COMPONENT

#include <memory>
#include <vector>

#include "Component.h"

namespace bird {

class Mesh;
class Material;

/**
 * 3D mesh rendering component attached to a WorldObject.
 *
 * Holds one or more shared Mesh references. Automatically registers itself
 * with the active GraphicsPipeline during init() and unregisters during deinit(),
 * submitting its meshes for rendering whenever its host entity is in an active scene.
 */
class MeshComponent : public Component {
   public:
	/**
	 * Creates a mesh component by loading a model asset via ResourceManager.
	 * @param path File path to the 3D model (e.g. .glb, .obj, .fbx).
	 */
	MeshComponent(std::string path);

	/**
	 * Creates a mesh component with an existing collection of shared meshes.
	 */
	MeshComponent(std::vector<_ref<Mesh>> meshes);

	/**
	 * Creates a mesh component containing a single shared mesh.
	 */
	MeshComponent(_ref<Mesh> meshes);

	/**
	 * Registers this component with the active GraphicsPipeline.
	 */
	void init() override;

	/**
	 * Unregisters this component from the active GraphicsPipeline.
	 */
	void deinit() override;

	void process(float delta);

	/** Returns the collection of meshes associated with this component. */
	std::vector<_ref<Mesh>>& getMeshes();

	/** @return Number of meshes (one material each). */
	int getMeshCount();

	/**
	 * Returns the material one mesh renders with: this component's override
	 * when set, otherwise the mesh's own material.
	 * @throws std::out_of_range for an invalid index.
	 */
	_ref<Material> getMaterial(int meshIndex);

	/**
	 * Overrides the material of one mesh for this component only; meshes
	 * loaded from one model are shared, so the mesh itself is untouched.
	 * Passing null restores the mesh's own material.
	 * @throws std::out_of_range for an invalid index.
	 */
	void setMaterial(int meshIndex, _ref<Material> material);

	/** Overrides the material of every mesh of this component. */
	void setAllMaterials(_ref<Material> material);

   protected:
	std::vector<_ref<Mesh>> m_meshes;
	/** Per-mesh overrides; empty or null entries fall back to the mesh. */
	std::vector<_ref<Material>> m_materialOverrides;
};

}  // namespace bird

#endif	// !H_BIRD_MESH_COMPONENT
