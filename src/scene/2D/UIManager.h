#ifndef BIRD_UIMANAGER_H
#define BIRD_UIMANAGER_H

#include <memory>

#include "../../util/Input.h"
#include "UIObject.h"

namespace bird {
class Mesh;

extern Input* INPUT;

class UIManager {
   public:
	UIManager();
	~UIManager();

	void init();
	void deinit();

	void process(float delta);
	void processTreeUI(UIObject* root);
	void processMouseUI(double mx, double my, int action);
	void setUpdateUIFlag();

	std::shared_ptr<Mesh> getUIMesh();
	UIObject* getRoot();

   private:
	void processMouseUI(UIObject* root, double mx, double my, int action);
	void updateUI();
	std::unique_ptr<UIObject> m_root;

	std::shared_ptr<Mesh> m_mesh;
	std::vector<float> m_vertices;
	std::vector<float> m_textureCoords;
	std::vector<float> m_colors;
	std::vector<uint32_t> m_indices;

	uint32_t m_vertexCount;
	bool m_shouldUpdateUI;
};

}  // namespace bird

#endif	// !BIRD_UIMANAGER_H
