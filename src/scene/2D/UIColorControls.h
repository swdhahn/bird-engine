#ifndef BIRD_UICOLORCONTROLS_H
#define BIRD_UICOLORCONTROLS_H

#include <functional>
#include <utility>

#include "UIObject.h"

namespace bird {

/**
 * Hue/saturation disc: hue runs around the circle (0 degrees = red at
 * the right, increasing counter-clockwise), saturation from the centre
 * (0) to the rim (1). Drawn at full value; value has its own slider.
 * Click or drag to pick; Left/Right change hue, Up/Down saturation
 * (Shift: larger steps).
 */
class UIColorWheel : public UIObject {
   public:
	static constexpr int kSegments = 96;

	explicit UIColorWheel(float diameter = 180.0f);

	/** Sets hue (degrees) and saturation without notifying. */
	void setHueSaturation(float hue, float saturation);
	float getHue() const { return m_hue; }
	float getSaturation() const { return m_saturation; }
	UIColorWheel& setOnChanged(std::function<void(float, float)> onChanged);

	/** {hue, saturation} under a local point (clamped to the rim). */
	std::pair<float, float> hueSaturationAt(Vector2 local) const;
	/** Local position of a hue/saturation pair. */
	Vector2 pointFor(float hue, float saturation) const;

	bool isFocusable() const override { return true; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override { return true; }
	bool onKey(const UIKeyEvent& event) override;
	void draw(UIDrawList& list) override;

   private:
	void pick(Vector2 local);
	void change(float hue, float saturation);
	Vector2 center() const { return getSize() * 0.5f; }
	float radius() const;

	float m_hue = 0.0f;
	float m_saturation = 0.0f;
	std::function<void(float, float)> m_onChanged;
};

/**
 * A 0..1 slider whose track is a colour gradient (value, alpha, or one RGB
 * channel). Vertical sliders put 1 at the top. Checkered tracks show
 * transparency. Arrows step by 1% (Shift: 10%).
 */
class UIColorSlider : public UIObject {
   public:
	explicit UIColorSlider(UIAxis axis = UIAxis::Horizontal);

	/** Sets the value (clamped) without notifying. */
	void setValue(float value);
	float getValue() const { return m_value; }
	UIColorSlider& setOnChanged(std::function<void(float)> onChanged);
	/** Track colours at value 0 and value 1. */
	void setGradient(const Vector4& atZero, const Vector4& atOne);
	UIColorSlider& setCheckered(bool checkered);
	/** Value under a local point. */
	float valueAt(Vector2 local) const;

	bool isFocusable() const override { return true; }
	UICursor cursor() const override { return UICursor::Hand; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override { return true; }
	bool onKey(const UIKeyEvent& event) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	void change(float value);

	UIAxis m_axis;
	float m_value = 0.0f;
	Vector4 m_atZero{0.0f, 0.0f, 0.0f, 1.0f};
	Vector4 m_atOne{1.0f};
	bool m_checkered = false;
	std::function<void(float)> m_onChanged;
};

/**
 * Colour preview. With an original colour set, the left half shows it and
 * clicking that half reverts to it.
 */
class UIColorSwatch : public UIObject {
   public:
	UIColorSwatch();

	void setColor(const Vector4& color);
	void setOriginal(const Vector4& original);
	const Vector4& getColor() const { return m_color; }
	UIColorSwatch& setOnRevert(std::function<void(const Vector4&)> onRevert);

	bool onMouseDown(const UIMouseEvent& event) override;
	UICursor cursor() const override { return UICursor::Hand; }
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	Vector4 m_color{1.0f};
	Vector4 m_original{1.0f};
	bool m_hasOriginal = false;
	std::function<void(const Vector4&)> m_onRevert;
};

namespace ui {
/** Shared checkerboard colours for transparency previews. */
void drawTransparencyChecker(UIDrawList& list, const UIRect& rect);
}  // namespace ui

}  // namespace bird

#endif	// !BIRD_UICOLORCONTROLS_H
