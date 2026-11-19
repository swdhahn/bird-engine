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

struct UIDrawable {
	Vector2 localOffset;
	Vector2 size;
	Vector3 color;
	bool visible = true;
	std::shared_ptr<Texture> texture;
};

class UIObject : public ScreenObject {
   public:
	UIObject(uint8_t drawableCount);
	~UIObject();

	/** Invoked on the top-most hit object when a mouse button goes down. */
	virtual void onPress(float mx, float my) {}
	/** Invoked on the pressed object on release, even outside its bounds. */
	virtual void onRelease(float mx, float my) {}
	/** Invoked on the pressed object, else the hovered one, on cursor motion. */
	virtual void onMove(float mx, float my) {}
	/** Invoked when the cursor enters (true) or leaves (false) the object. */
	virtual void onHover(bool hovered) {}

	virtual void deinit() override {}
	virtual void init() override {}

	Vector2 getBounds() const;
	bool enabled() const;

	/**
	 * Refreshes the local matrix if stale and composes the world matrix.
	 * @param parentWorldMatrix - the parent's object-to-screen transform.
	 */
	void updateWorldMatrix(const Matrix3& parentWorldMatrix);
	const Matrix3& getWorldMatrix() const;
	/** @return true when the screen point lies inside this object's quad. */
	bool containsPoint(const Vector2& screenPoint) const;

	UIDrawable* getDrawables();
	uint8_t getDrawableCount();
	std::vector<std::unique_ptr<UIObject>>& getChildren();

	void addChild(std::unique_ptr<UIObject> child);

   protected:
	std::vector<std::unique_ptr<UIObject>> m_children;
	std::unique_ptr<UIDrawable[]> m_drawables;

	std::shared_ptr<UITheme> m_theme;
	Matrix3 m_worldMatrix;
	Vector2 m_bounds;
	uint8_t m_drawableCount;
	bool m_enabled;
};

}  // namespace bird

#endif	// !BIRD_UIOBJECT_H
