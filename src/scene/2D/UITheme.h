#ifndef BIRD_UITHEME_H
#define BIRD_UITHEME_H

#include <stdint.h>

#include <memory>

#include "../../util/Math.h"

namespace bird {

/** Colours (RGBA, 0..1) and metrics shared by every widget. */
struct UITheme {
	// --- Surfaces ---
	Vector4 backgroundColor{0.118f, 0.118f, 0.118f, 1.0f};	 // #1E1E1E
	Vector4 surfaceColor{0.176f, 0.176f, 0.176f, 1.0f};		 // #2D2D2D
	Vector4 panelColor{0.141f, 0.145f, 0.153f, 0.96f};		 // window body
	Vector4 titleBarColor{0.098f, 0.102f, 0.110f, 1.0f};
	Vector4 inputColor{0.086f, 0.090f, 0.098f, 1.0f};  // text box, track

	// --- Text ---
	Vector4 textColor{0.878f, 0.878f, 0.878f, 1.0f};	 // #E0E0E0
	Vector4 mutedTextColor{0.600f, 0.612f, 0.635f, 1.0f};  // placeholder
	Vector4 disabledText{0.459f, 0.459f, 0.459f, 1.0f};	   // #757575

	// --- Accent and interaction states ---
	Vector4 accentColor{0.000f, 0.478f, 0.800f, 1.0f};	// #007ACC
	Vector4 hoverColor{0.243f, 0.243f, 0.243f, 1.0f};	// #3E3E3E
	Vector4 activeColor{0.306f, 0.306f, 0.306f, 1.0f};	// #4E4E4E
	Vector4 disabledColor{0.145f, 0.145f, 0.145f, 1.0f};
	Vector4 selectionColor{0.000f, 0.478f, 0.800f, 0.45f};
	Vector4 caretColor{0.878f, 0.878f, 0.878f, 1.0f};
	Vector4 scrollbarColor{0.400f, 0.400f, 0.420f, 0.70f};
	Vector4 backdropColor{0.0f, 0.0f, 0.0f, 0.55f};	 // behind modal dialogs
	Vector4 tooltipColor{0.050f, 0.052f, 0.058f, 0.95f};

	// --- Borders ---
	Vector4 borderColor{0.271f, 0.271f, 0.271f, 1.0f};	 // #454545
	Vector4 focusBorderColor{0.000f, 0.478f, 0.800f, 1.0f};
	float borderWidth = 1.0f;
	float borderRadius = 4.0f;	// reserved: corners are square for now

	// --- Typography and layout ---
	float fontSize = 14.0f;
	float titleFontSize = 15.0f;
	float padding = 8.0f;	   // inside panels, buttons and inputs
	float itemSpacing = 6.0f;  // default gap between stacked items
	float controlHeight = 28.0f;
	float titleBarHeight = 30.0f;
	float scrollbarWidth = 8.0f;
	float checkBoxSize = 18.0f;
	float sliderThumbWidth = 10.0f;
	float caretBlinkSeconds = 0.53f;
	float tooltipDelaySeconds = 0.5f;
};

/** Shared theme applied to every UIObject that does not receive its own. */
inline std::shared_ptr<UITheme> defaultUITheme() {
	static const std::shared_ptr<UITheme> theme = std::make_shared<UITheme>();
	return theme;
}

}  // namespace bird

#endif	// !BIRD_UITHEME_H
