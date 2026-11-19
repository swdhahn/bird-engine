#ifndef BIRD_UIPROGRESSBAR_H
#define BIRD_UIPROGRESSBAR_H

#include "UIObject.h"

namespace bird {

/** Read-only bar showing a 0..1 fraction, optionally with a percentage. */
class UIProgressBar : public UIObject {
   public:
	explicit UIProgressBar(float value = 0.0f);

	/** Clamped to 0..1. */
	UIProgressBar& setValue(float value);
	float getValue() const { return m_value; }
	UIProgressBar& setShowPercentage(bool show);

	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	float m_value;
	bool m_showPercentage = true;
};

}  // namespace bird

#endif	// !BIRD_UIPROGRESSBAR_H
