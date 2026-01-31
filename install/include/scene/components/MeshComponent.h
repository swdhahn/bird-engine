#ifndef H_BIRD_MESH_COMPONENT
#define H_BIRD_MESH_COMPONENT

#include <memory>
#include <vector>

#include "Component.h"

namespace bird {

class MeshComponent : public Component {
   public:
	MeshComponent(std::string path);
	MeshComponent(std::vector<std::shared_ptr<Mesh>> meshes);

	void init();
	void deinit();

	std::vector<std::shared_ptr<Mesh>>& getMeshes();

   protected:
	std::vector<std::shared_ptr<Mesh>> m_meshes;
};

}  // namespace bird
#endif	// !H_BIRD_MESH_COMPONENT
