#ifndef H_BIRD_MESH_COMPONENT
#define H_BIRD_MESH_COMPONENT

#include <memory>
#include <vector>

#include "Component.h"

namespace bird {

class Mesh;

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

   protected:
	std::vector<_ref<Mesh>> m_meshes;
};

}  // namespace bird

#endif	// !H_BIRD_MESH_COMPONENT
