#ifndef BIRD_UITHEME_H
#define BIRD_UITHEME_H

#include <stdint.h>

#include <memory>

#include "../../util/Math.h"

namespace bird {

struct UITheme {
	// --- Core Colors ---
	//
	// The window background, Buttons, panels, cards (Surface)
	Vector3 backgroundColor = Vector3(0.118f, 0.118f, 0.118f);	// #1E1E1E

	// Buttons, panels, cards (Surface)
	Vector3 surfaceColor = Vector3(0.176f, 0.176f, 0.176f);	 // #2D2D2D

	// Primary text
	Vector3 textColor = Vector3(0.878f, 0.878f, 0.878f);  // #E0E0E0

	// Primary brand color (Accent)
	Vector3 accentColor = Vector3(0.000f, 0.478f, 0.800f);	// #007ACC

	// --- Interaction States ---
	Vector3 hoverColor = Vector3(0.243f, 0.243f, 0.243f);	  // #3E3E3E
	Vector3 activeColor = Vector3(0.306f, 0.306f, 0.306f);	  // #4E4E4E
	Vector3 disabledColor = Vector3(0.145f, 0.145f, 0.145f);  // #252525
	Vector3 disabledText = Vector3(0.459f, 0.459f, 0.459f);	  // #757575

	// --- Borders ---
	Vector3 borderColor = Vector3(0.271f, 0.271f, 0.271f);	// #454545

	float borderWidth = 1.0f;	// 0.0f for no border
	float borderRadius = 4.0f;	// Corner rounding

	// --- Typography & Layout ---
	float fontSize = 14.0f;	   // Base font size
	float padding = 8.0f;	   // Internal spacing (text to border)
	float itemSpacing = 4.0f;  // Gap between elements
};

/** Shared theme applied to every UIObject that does not receive its own. */
inline std::shared_ptr<UITheme> defaultUITheme() {
	static const std::shared_ptr<UITheme> theme = std::make_shared<UITheme>();
	return theme;
}

}  // namespace bird

#endif	// !BIRD_UITHEME_H
