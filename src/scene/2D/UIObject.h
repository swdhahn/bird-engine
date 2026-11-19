#ifndef BIRD_UIOBJECT_H
#define BIRD_UIOBJECT_H

#include <memory>
#include <vector>

#include "../../renders/generic/Texture.h"
#include "ScreenObject.h"
#include "UITheme.h"

namespace bird {

struct UIAlignment {
	static constexpr Vector2 TopLeft{0.0f, 0.0f};
	static constexpr Vector2 Center{0.5f, 0.5f};
	static constexpr Vector2 BottomRight{1.0f, 1.0f};
};

/**
 * One quad of a UI object, in the object's unit space ([0, 1] spans the
 * object's scale). Untextured quads sample the font atlas's white texel.
 */
struct UIDrawable {
	Vector2 localOffset;
	Vector2 size;
	Vector3 color;
	bool visible = true;
	std::shared_ptr<Texture> texture;
	bool textured = false;	// true: sample the atlas between uvMin and uvMax
	Vector2 uvMin{0.0f, 0.0f};
	Vector2 uvMax{0.0f, 0.0f};
};

class UIObject : public ScreenObject {
   public:
	explicit UIObject(size_t drawableCount);
	~UIObject();

	/** Invoked on the top-most hit object when a mouse button goes down. */
	virtual void onPress(float mx, float my) {}
	/** Invoked on the pressed object on release, even outside its bounds. */
	virtual void onRelease(float mx, float my) {}
	/** Invoked on the pressed object, else the hovered one, on cursor motion. */
	virtual void onMove(float mx, float my) {}
	/** Invoked when the cursor enters (true) or leaves (false) the object. */
	virtual void onHover(bool hovered) {}
	/**
	 * Invoked by UIManager before transforms are composed during a rebuild,
	 * so that an object can size itself or arrange its children.
	 */
	virtual void layout() {}

	virtual void deinit() override {}
	virtual void init() override {}

	Vector2 getBounds() const;
	bool enabled() const;
	/** @return false for objects that never receive mouse events. */
	bool interactive() const;

	/**
	 * Composes this object's transforms from its parent's basis. Position
	 * and rotation are inherited; scale is not, so children are placed in
	 * pixels relative to the parent's top-left corner.
	 * @param parentBasis - the parent's getChildBasis().
	 * @param parentSize - the parent's size in pixels, for anchoring.
	 */
	void updateWorldMatrix(const Matrix3& parentBasis,
						   const Vector2& parentSize);

	/**
	 * Sets the point on the parent this object is placed from, as a fraction
	 * of the parent's size (see UIAlignment). Position becomes an offset
	 * from that point. Defaults to UIAlignment::TopLeft.
	 */
	void setAnchor(const Vector2& anchor);
	const Vector2& getAnchor() const;
	/**
	 * Sets the point on this object, as a fraction of its own size, that
	 * lands on the anchor. Defaults to UIAlignment::TopLeft.
	 */
	void setPivot(const Vector2& pivot);
	const Vector2& getPivot() const;
	/** @return the unit-quad-to-screen transform (includes scale). */
	const Matrix3& getWorldMatrix() const;
	/** @return the transform handed to children (excludes scale). */
	const Matrix3& getChildBasis() const;
	/** @return true when the screen point lies inside this object's quad. */
	bool containsPoint(const Vector2& screenPoint) const;

	UIDrawable* getDrawables();
	size_t getDrawableCount() const;
	std::vector<std::unique_ptr<UIObject>>& getChildren();

	void addChild(std::unique_ptr<UIObject> child);

   protected:
	std::vector<std::unique_ptr<UIObject>> m_children;
	std::vector<UIDrawable> m_drawables;

	std::shared_ptr<UITheme> m_theme;
	Matrix3 m_worldMatrix;
	Matrix3 m_childBasis;
	Vector2 m_anchor;
	Vector2 m_pivot;
	Vector2 m_bounds;
	bool m_enabled;
	bool m_interactive;
};

}  // namespace bird

#endif	// !BIRD_UIOBJECT_H
