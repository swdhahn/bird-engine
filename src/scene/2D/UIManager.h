#ifndef BIRD_UIMANAGER_H
#define BIRD_UIMANAGER_H

#include <memory>

#include "../../util/Input.h"
#include "UIObject.h"

namespace bird {
class Mesh;
class Font;

extern Input* INPUT;

class UIManager {
   public:
	UIManager();
	~UIManager();

	/** Font baked by BuildTools and passed to init() by Application. */
	static constexpr const char* kDefaultFontPath =
		"assets/fonts/DejaVuSans.ttf";

	/**
	 * Creates GPU buffers and adopts the UI font, whose atlas also supplies
	 * the white texel for solid quads.
	 * @param font - required; see kDefaultFontPath.
	 * @throws std::invalid_argument when font is nullptr.
	 */
	void init(std::shared_ptr<Font> font);
	void deinit();

	/** Rebuilds the batched UI mesh when the tree has been marked dirty. */
	void process(float delta);
	/**
	 * Routes a mouse event to the UI tree.
	 * @param mx, my - cursor position in window pixels.
	 * @param action - ACTION_NONE (motion), ACTION_PRESS or ACTION_RELEASE.
	 */
	void processMouseUI(double mx, double my, int action);
	void setUpdateUIFlag();

	std::shared_ptr<Mesh> getUIMesh();
	const std::shared_ptr<Font>& getFont() const;
	/** @return the number of indices currently valid in the UI mesh. */
	uint32_t getIndexCount() const;
	UIObject* getRoot();

	/**
	 * Finds the top-most enabled, interactive, drawable object under a point. Later
	 * siblings and children render above earlier ones, so the search runs
	 * in reverse draw order.
	 * @return the hit object, or nullptr.
	 */
	static UIObject* findTopmostAt(UIObject* root, const Vector2& screenPoint);

   private:
	void updateUI();
	void processTreeUI(UIObject* object, const Matrix3& parentBasis);
	std::unique_ptr<UIObject> m_root;

	std::shared_ptr<Mesh> m_mesh;
	std::shared_ptr<Font> m_font;
	std::vector<float> m_vertices;
	std::vector<float> m_textureCoords;
	std::vector<float> m_colors;
	std::vector<uint32_t> m_indices;

	UIObject* m_hovered;
	UIObject* m_pressed;

	uint32_t m_vertexCount;
	uint32_t m_indexCount;
	bool m_shouldUpdateUI;
};

}  // namespace bird

#endif	// !BIRD_UIMANAGER_H
