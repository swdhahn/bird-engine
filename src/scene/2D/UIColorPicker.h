#ifndef BIRD_UICOLORPICKER_H
#define BIRD_UICOLORPICKER_H

#include <array>
#include <functional>
#include <string>

#include "UIButton.h"
#include "UIColor.h"
#include "UIStack.h"

namespace bird {

class UIColorWheel;
class UIColorSlider;
class UIColorSwatch;
class UINumberField;
class UITextBox;
class UILabel;

/**
 * Full colour picker. Every control edits the same colour and all others
 * follow immediately:
 *
 * - Hue/saturation wheel, vertical value bar and alpha bar.
 * - R, G, B gradient sliders (each track shows that channel's range).
 * - Numeric H (0-360), S, V (0-100 %) and R, G, B (0-255), A (0-100 %)
 *   fields: type, step with arrows/wheel, or drag the steppers.
 * - Hex field: "#RGB", "#RGBA", "#RRGGBB" or "#RRGGBBAA" ('#' optional),
 *   applied live once valid; Enter normalizes it.
 * - Copy (puts "#RRGGBB[AA]" on the clipboard) and Paste buttons.
 * - Before/after swatch: click the original half to revert.
 *
 * The colour is held as HSV so hue survives greys and black.
 */
class UIColorPicker : public UIStack {
   public:
	enum class Channel : uint8_t { H, S, V, R, G, B, A };

	explicit UIColorPicker(const Vector4& rgba = Vector4(1.0f), bool showAlpha = true);

	/** Sets the colour and the swatch's original; does not notify. */
	void setColor(const Vector4& rgba);
	Vector4 getColor() const;
	const color::HSV& getHSV() const { return m_hsv; }
	float getAlpha() const { return m_alpha; }
	std::string getHex() const;
	/** Applies a hex string. @return false (and no change) when invalid. */
	bool setHex(const std::string& hex);
	/** Called on every user change with the new RGBA colour. */
	UIColorPicker& setOnChanged(std::function<void(const Vector4&)> onChanged);

	void copyHex();
	bool pasteHex();

	UIColorWheel* getWheel() const { return m_wheel; }
	UIColorSlider* getValueSlider() const { return m_valueBar; }
	UIColorSlider* getAlphaSlider() const { return m_alphaBar; }
	UIColorSlider* getChannelSlider(Channel channel) const;
	UINumberField* getField(Channel channel) const;
	UITextBox* getHexField() const { return m_hexField; }
	UIColorSwatch* getSwatch() const { return m_swatch; }
	const std::string& getStatus() const;

   private:
	/** Commits a new colour from a control; `source` is not re-synced. */
	void apply(const color::HSV& hsv, float alpha, const UIObject* source);
	void applyRgb(const Vector3& rgb, const UIObject* source);
	void sync(const UIObject* source);
	void setStatus(const std::string& text);
	Vector3 rgb() const;

	color::HSV m_hsv;
	float m_alpha;
	bool m_syncing = false;
	std::function<void(const Vector4&)> m_onChanged;

	UIColorWheel* m_wheel;
	UIColorSlider* m_valueBar;
	UIColorSlider* m_alphaBar;
	std::array<UIColorSlider*, 3> m_rgbSliders{};
	std::array<UINumberField*, 7> m_fields{};
	UITextBox* m_hexField;
	UIColorSwatch* m_swatch;
	UILabel* m_status;
};

/**
 * Compact colour input: a swatch button that opens a UIColorPicker in a
 * popup. Changes apply live; clicking outside closes the popup.
 */
class UIColorButton : public UIButton {
   public:
	explicit UIColorButton(const Vector4& rgba = Vector4(1.0f));

	void setColor(const Vector4& rgba);
	const Vector4& getColor() const { return m_color; }
	UIColorButton& setOnColorChanged(std::function<void(const Vector4&)> onChanged);
	UIColorPicker* getOpenPicker() const { return m_picker; }

	void onPopupClosed(UIObject* popup) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	void openPicker();

	Vector4 m_color;
	std::function<void(const Vector4&)> m_onColorChanged;
	UIObject* m_popup = nullptr;
	UIColorPicker* m_picker = nullptr;
};

}  // namespace bird

#endif	// !BIRD_UICOLORPICKER_H
