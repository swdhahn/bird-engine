#ifndef H_BIRD_MESH_COMPONENT
#define H_BIRD_MESH_COMPONENT

#include <memory>
#include <vector>

#include "Component.h"

namespace bird {

class MeshComponent : public Component {
   public:
	MeshComponent(std::string path);
	MeshComponent(std::vector<_ref<Mesh>> meshes);
	MeshComponent(_ref<Mesh> meshes);

	void init();
	void deinit();
	void process(float delta);

	std::vector<_ref<Mesh>>& getMeshes();

   protected:
	std::vector<_ref<Mesh>> m_meshes;
};

}  // namespace bird
#endif	// !H_BIRD_MESH_COMPONENT
