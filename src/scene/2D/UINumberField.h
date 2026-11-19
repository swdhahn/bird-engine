#ifndef BIRD_UINUMBERFIELD_H
#define BIRD_UINUMBERFIELD_H

#include <functional>

#include "UITextBox.h"

namespace bird {

/**
 * Numeric input: a text field plus up/down steppers.
 *
 * - Typing accepts digits, sign, decimal point and exponent; the value is
 *   committed on Enter or when focus leaves, and invalid text reverts.
 * - Up/Down keys and the mouse wheel step (Shift: 10 steps).
 * - Click a stepper to step; drag vertically on the steppers to scrub
 *   (one step per kScrubPixels).
 * Values are clamped to [min, max] and snapped to the step from min.
 */
class UINumberField : public UITextBox {
   public:
	static constexpr float kStepperWidth = 18.0f;
	static constexpr float kScrubPixels = 6.0f;

	UINumberField(double min = 0.0, double max = 100.0, double value = 0.0);

	/** @throws std::invalid_argument when max <= min. */
	UINumberField& setRange(double min, double max);
	/** Step for keys, wheel and scrubbing; 0 disables snapping. */
	UINumberField& setStep(double step);
	/** Digits shown after the decimal point. */
	UINumberField& setPrecision(int digits);
	/** Clamps, snaps, reformats; notifies onChanged on change. */
	UINumberField& setValue(double value);
	UINumberField& setOnValueChanged(std::function<void(double)> onChanged);

	double getValue() const { return m_value; }
	/** Parses the current text and commits it, or reverts when invalid. */
	void commit();
	void stepBy(double steps);

	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onScroll(const UIScrollEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	void onFocus(bool focused) override;
	UICursor cursor() const override;
	void draw(UIDrawList& list) override;

   protected:
	bool acceptsCharacter(uint32_t codepoint) const override;
	Vector2 measureContent(Vector2 available) override;

   private:
	double snap(double value) const;
	void refreshText();
	double effectiveStep() const;
	bool inSteppers(Vector2 local) const;

	double m_min, m_max, m_value;
	double m_step = 1.0;
	int m_precision = 0;
	std::function<void(double)> m_onValueChanged;
	bool m_scrubbing = false;
	float m_scrubStartY = 0.0f;
	double m_scrubStartValue = 0.0;
	Vector2 m_lastLocalMouse{-1.0f};
};

}  // namespace bird

#endif	// !BIRD_UINUMBERFIELD_H
