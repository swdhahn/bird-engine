#ifndef BIRD_UIOBJECT_H
#define BIRD_UIOBJECT_H

#include <memory>
#include <vector>

#include "../../renders/generic/Texture.h"
#include "ScreenObject.h"
#include "UITheme.h"

namespace bird {

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

	virtual void onPress(float mx, float my) {}
	virtual void onRelease(float mx, float my) {}
	virtual void onMove(float mx, float my) {}

	virtual void deinit() override {}
	virtual void init() override {}

	Vector2 getBounds() const;
	bool enabled() const;

	UIDrawable* getDrawables();
	uint8_t getDrawableCount();
	std::vector<std::unique_ptr<UIObject>>& getChildren();

	void addChild(std::unique_ptr<UIObject> child);

   protected:
	std::vector<std::unique_ptr<UIObject>> m_children;
	std::unique_ptr<UIDrawable[]> m_drawables;

	std::shared_ptr<UITheme> m_theme;
	Vector2 m_bounds;
	uint8_t m_drawableCount;
	bool m_enabled;
};

}  // namespace bird

#endif	// !BIRD_UIOBJECT_H
