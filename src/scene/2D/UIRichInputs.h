#ifndef BIRD_UIRICHINPUTS_H
#define BIRD_UIRICHINPUTS_H

#include <functional>
#include <memory>
#include <string>

#include "UIObject.h"

namespace bird {

class Texture;

/**
 * Slider with two thumbs selecting [low, high]. A press grabs the nearer
 * thumb (which becomes the keyboard-active one); thumbs cannot cross and
 * keep at least minGap apart. Left/Right step the active thumb, Home/End
 * send it to its limit, Space switches thumbs.
 */
class UIRangeSlider : public UIObject {
   public:
	UIRangeSlider(float min = 0.0f, float max = 1.0f, float low = 0.25f,
				  float high = 0.75f);

	/** @throws std::invalid_argument when max <= min. */
	UIRangeSlider& setRange(float min, float max);
	UIRangeSlider& setStep(float step);
	UIRangeSlider& setMinGap(float gap);
	/** Clamps, orders and snaps; notifies on change. */
	UIRangeSlider& setValues(float low, float high);
	UIRangeSlider& setOnChanged(std::function<void(float, float)> onChanged);

	float getLow() const { return m_low; }
	float getHigh() const { return m_high; }
	/** 0 = low thumb, 1 = high thumb. */
	int getActiveThumb() const { return m_active; }
	float valueAt(float localX) const;

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
	float snap(float value) const;
	float xOf(float value) const;
	void moveActive(float value);

	float m_min, m_max, m_low, m_high;
	float m_step = 0.0f;
	float m_minGap = 0.0f;
	int m_active = 0;
	std::function<void(float, float)> m_onChanged;
};

/**
 * "Press a key" control for game bindings. Click, Enter or Space starts
 * capturing; the next key press becomes the binding (any key, including
 * Tab and modifiers). Escape cancels, Backspace/Delete clears to unbound,
 * clicking elsewhere cancels. While capturing it owns the keyboard, so
 * the key does not also reach game actions.
 */
class UIKeyBindField : public UIObject {
   public:
	static constexpr int kUnbound = -1;

	explicit UIKeyBindField(int keyCode = kUnbound);

	UIKeyBindField& setKey(int keyCode);
	int getKey() const { return m_key; }
	bool isCapturing() const { return m_capturing; }
	void startCapture();
	/** Display text: the key name, "Unbound", or the capture prompt. */
	std::string getDisplayText() const;
	UIKeyBindField& setOnChanged(std::function<void(int)> onChanged);

	bool isFocusable() const override { return true; }
	bool capturesKeyboard() const override { return m_capturing; }
	UICursor cursor() const override { return UICursor::Hand; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override { return true; }
	bool onKey(const UIKeyEvent& event) override;
	void onFocus(bool focused) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	int m_key;
	bool m_capturing = false;
	std::function<void(int)> m_onChanged;
};

/** How a UIImage maps its texture into its rect. */
enum class UIImageFit : uint8_t {
	Stretch,  // fill the rect, ignoring aspect ratio
	Contain,  // whole image visible, letterboxed
	Cover	  // rect fully covered, image cropped
};

/** Displays a texture with a fit mode, tint and optional rounded corners. */
class UIImage : public UIObject {
   public:
	explicit UIImage(std::shared_ptr<Texture> texture = nullptr,
					 UIImageFit fit = UIImageFit::Contain);

	UIImage& setTexture(std::shared_ptr<Texture> texture);
	const std::shared_ptr<Texture>& getTexture() const { return m_texture; }
	UIImage& setFit(UIImageFit fit);
	UIImage& setTint(const Vector4& tint);
	UIImage& setCornerRadius(float radius);

	/** Destination rect and UV range for the current fit. */
	struct Placement {
		UIRect rect;
		Vector2 uvMin;
		Vector2 uvMax;
	};
	Placement computePlacement() const;

	void draw(UIDrawList& list) override;

   protected:
	/** Natural size: the texture's pixel size (or 0 without one). */
	Vector2 measureContent(Vector2 available) override;

   private:
	std::shared_ptr<Texture> m_texture;
	UIImageFit m_fit;
	Vector4 m_tint{1.0f};
	float m_radius = 0.0f;
};

/**
 * Two panes separated by a draggable divider. Drag the divider to resize
 * (each pane keeps at least minPaneSize); double-click it to restore the
 * initial ratio. Add content to getFirst() and getSecond().
 */
class UISplitter : public UIObject {
   public:
	static constexpr float kDividerThickness = 6.0f;

	explicit UISplitter(UIAxis axis = UIAxis::Horizontal, float ratio = 0.5f);

	UIObject* getFirst() const { return m_first; }
	UIObject* getSecond() const { return m_second; }
	/** Fraction of the space given to the first pane (clamped by mins). */
	UISplitter& setRatio(float ratio);
	float getRatio() const { return m_ratio; }
	UISplitter& setMinPaneSize(float pixels);
	UISplitter& setOnRatioChanged(std::function<void(float)> onChanged);
	/** Divider rect in local space. */
	UIRect dividerRect() const;

	UICursor cursor() const override;
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;
	void arrangeChildren() override;

   private:
	float available() const;
	float clampedRatio(float ratio) const;

	UIAxis m_axis;
	float m_ratio;
	float m_initialRatio;
	float m_minPane = 40.0f;
	bool m_dragging = false;
	float m_dragOffset = 0.0f;	// pointer offset within the divider
	Vector2 m_lastLocalMouse{-1.0f};
	UIObject* m_first;
	UIObject* m_second;
	std::function<void(float)> m_onRatioChanged;
};

}  // namespace bird

#endif	// !BIRD_UIRICHINPUTS_H
