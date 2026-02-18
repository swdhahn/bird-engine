#ifndef BIRD_BUTTON_H
#define BIRD_BUTTON_H

#include "UIObject.h"
namespace bird {

class UIButton : public UIObject {
   public:
	UIButton();
	~UIButton();

	void onPress(float mx, float my) override;
	void onRelease(float mx, float my) override;
	void onMove(float mx, float my) override;
};

}  // namespace bird

#endif	// !BIRD_BUTTON_H
