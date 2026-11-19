#include "UIProgressBar.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "UITextUtil.h"

namespace bird {

namespace {
constexpr float kDefaultWidth = 160.0f;
constexpr float kDefaultHeight = 18.0f;
}  // namespace

UIProgressBar::UIProgressBar(float value)
	: m_value(std::clamp(value, 0.0f, 1.0f)) {}

UIProgressBar& UIProgressBar::setValue(float value) {
	const float clamped = std::clamp(value, 0.0f, 1.0f);
	if (clamped != m_value) {
		m_value = clamped;
		markVisualDirty();
	}
	return *this;
}

UIProgressBar& UIProgressBar::setShowPercentage(bool show) {
	m_showPercentage = show;
	markVisualDirty();
	return *this;
}

Vector2 UIProgressBar::measureContent(Vector2 /*available*/) {
	return Vector2(kDefaultWidth, kDefaultHeight);
}

void UIProgressBar::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	const float radius = bounds.size.y * 0.5f;
	list.roundedRect(bounds, t.inputColor, radius, t.borderWidth, t.borderColor);
	if (m_value > 0.0f) {
		// Never narrower than the bar is tall, so the pill stays round.
		const float width = std::max(bounds.size.x * m_value, bounds.size.y);
		list.roundedRect({Vector2(0.0f), Vector2(width, bounds.size.y)},
						 t.accentColor, radius);
	}
	if (m_showPercentage) {
		const std::string label =
			std::to_string(static_cast<int>(std::round(m_value * 100.0f))) + "%";
		ui::drawTextCentered(list, font(), label, t.fontSize * 0.85f, bounds,
							 t.textColor);
	}
}

}  // namespace bird
