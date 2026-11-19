#include "UIRichInputs.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "../../renders/generic/Texture.h"
#include "UIManager.h"
#include "UITextUtil.h"

namespace bird {

// --------------------------------------------------------- UIRangeSlider

namespace {
constexpr float kRangeWidth = 180.0f;
constexpr float kTrackHeight = 4.0f;
}  // namespace

UIRangeSlider::UIRangeSlider(float min, float max, float low, float high)
	: m_min(min), m_max(max), m_low(min), m_high(max) {
	if (max <= min) throw std::invalid_argument("UIRangeSlider: max <= min");
	m_interactive = true;
	m_low = std::clamp(std::min(low, high), min, max);
	m_high = std::clamp(std::max(low, high), min, max);
}

UIRangeSlider& UIRangeSlider::setRange(float min, float max) {
	if (max <= min) throw std::invalid_argument("UIRangeSlider: max <= min");
	m_min = min;
	m_max = max;
	return setValues(m_low, m_high);
}

UIRangeSlider& UIRangeSlider::setStep(float step) {
	m_step = std::max(step, 0.0f);
	return setValues(m_low, m_high);
}

UIRangeSlider& UIRangeSlider::setMinGap(float gap) {
	m_minGap = std::clamp(gap, 0.0f, m_max - m_min);
	return setValues(m_low, m_high);
}

float UIRangeSlider::snap(float value) const {
	value = std::clamp(value, m_min, m_max);
	if (m_step > 0.0f) {
		value = std::clamp(m_min + std::round((value - m_min) / m_step) * m_step,
						   m_min, m_max);
	}
	return value;
}

UIRangeSlider& UIRangeSlider::setValues(float low, float high) {
	float lo = snap(std::min(low, high));
	float hi = snap(std::max(low, high));
	if (hi - lo < m_minGap) {
		// Push the pair apart, preferring to keep the low thumb in place.
		hi = std::min(lo + m_minGap, m_max);
		lo = std::max(hi - m_minGap, m_min);
	}
	if (lo != m_low || hi != m_high) {
		m_low = lo;
		m_high = hi;
		markVisualDirty();
		if (m_onChanged) m_onChanged(m_low, m_high);
	}
	return *this;
}

UIRangeSlider& UIRangeSlider::setOnChanged(
	std::function<void(float, float)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

float UIRangeSlider::xOf(float value) const {
	const float start = getPadding().left + theme().sliderThumbWidth * 0.5f;
	const float length = std::max(contentSize().x - theme().sliderThumbWidth, 1.0f);
	return start + (value - m_min) / (m_max - m_min) * length;
}

float UIRangeSlider::valueAt(float localX) const {
	const float start = getPadding().left + theme().sliderThumbWidth * 0.5f;
	const float length = std::max(contentSize().x - theme().sliderThumbWidth, 1.0f);
	return m_min + std::clamp((localX - start) / length, 0.0f, 1.0f) * (m_max - m_min);
}

void UIRangeSlider::moveActive(float value) {
	// A thumb cannot pass its partner (minus the minimum gap).
	if (m_active == 0) {
		setValues(std::min(snap(value), m_high - m_minGap), m_high);
	} else {
		setValues(m_low, std::max(snap(value), m_low + m_minGap));
	}
}

bool UIRangeSlider::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	const float x = toLocal(event.position).x;
	const float toLow = std::abs(x - xOf(m_low));
	const float toHigh = std::abs(x - xOf(m_high));
	// On overlap, pick by direction so the pair can always separate.
	m_active = toLow < toHigh ? 0 : toHigh < toLow ? 1 : (x < xOf(m_low) ? 0 : 1);
	moveActive(valueAt(x));
	return true;
}

bool UIRangeSlider::onMouseMove(const UIMouseEvent& event) {
	if (!isPressed()) return false;
	moveActive(valueAt(toLocal(event.position).x));
	return true;
}

bool UIRangeSlider::onKey(const UIKeyEvent& event) {
	const float step = m_step > 0.0f ? m_step : (m_max - m_min) * 0.05f;
	const float current = m_active == 0 ? m_low : m_high;
	switch (event.key) {
		case UIKey::Left:
		case UIKey::Down:
			moveActive(current - step);
			return true;
		case UIKey::Right:
		case UIKey::Up:
			moveActive(current + step);
			return true;
		case UIKey::Home:
			moveActive(m_min);
			return true;
		case UIKey::End:
			moveActive(m_max);
			return true;
		case UIKey::Space:
			m_active = 1 - m_active;
			markVisualDirty();
			return true;
		default:
			return false;
	}
}

Vector2 UIRangeSlider::measureContent(Vector2 /*available*/) {
	return Vector2(kRangeWidth, theme().controlHeight);
}

void UIRangeSlider::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const float cy = getPadding().top + contentSize().y * 0.5f;
	const float r = kTrackHeight * 0.5f;
	const float x0 = xOf(m_min), x1 = xOf(m_max);
	const float lo = xOf(m_low), hi = xOf(m_high);
	list.roundedRect({Vector2(x0, cy - r), Vector2(x1 - x0, kTrackHeight)}, t.inputColor, r);
	list.roundedRect({Vector2(lo, cy - r), Vector2(std::max(hi - lo, kTrackHeight), kTrackHeight)},
					 isEffectivelyEnabled() ? t.accentColor : t.disabledText, r);
	const float d = t.sliderThumbWidth;
	for (int i = 0; i < 2; i++) {
		const float x = i == 0 ? lo : hi;
		const bool active = isFocused() && m_active == i;
		list.roundedRect({Vector2(x, cy) - Vector2(d * 0.5f), Vector2(d)},
						 isHovered() || isPressed() ? t.textColor : t.mutedTextColor,
						 d * 0.5f, active ? 2.0f : 0.0f, t.focusBorderColor);
	}
}

// -------------------------------------------------------- UIKeyBindField

UIKeyBindField::UIKeyBindField(int keyCode) : m_key(keyCode) {
	m_interactive = true;
	setPadding(UIInsets::symmetric(theme().padding, theme().padding * 0.5f));
	UISizeRule height = UISizeRule::fit();
	height.min = theme().controlHeight;
	setHeight(height);
}

UIKeyBindField& UIKeyBindField::setKey(int keyCode) {
	m_key = keyCode;
	markLayoutDirty();
	return *this;
}

UIKeyBindField& UIKeyBindField::setOnChanged(std::function<void(int)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

void UIKeyBindField::startCapture() {
	m_capturing = true;
	if (UIManager* manager = getManager()) manager->setFocus(this);
	markLayoutDirty();
}

std::string UIKeyBindField::getDisplayText() const {
	if (m_capturing) return "Press a key...";
	if (m_key == kUnbound) return "Unbound";
	UIPlatform* p = platform();
	return p ? p->keyName(m_key) : "Key " + std::to_string(m_key);
}

bool UIKeyBindField::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	startCapture();
	return true;
}

bool UIKeyBindField::onKey(const UIKeyEvent& event) {
	if (!m_capturing) {
		if (event.key == UIKey::Enter || event.key == UIKey::Space) {
			startCapture();
			return true;
		}
		return false;
	}
	// Capturing: every key is consumed, so none reaches focus traversal.
	m_capturing = false;
	markLayoutDirty();
	if (event.key == UIKey::Escape) return true;  // cancel
	const int key = (event.key == UIKey::Backspace || event.key == UIKey::Delete)
						? kUnbound
						: event.keyCode;
	if (key != m_key) {
		m_key = key;
		if (m_onChanged) m_onChanged(m_key);
	}
	return true;
}

void UIKeyBindField::onFocus(bool focused) {
	if (!focused && m_capturing) {
		m_capturing = false;  // clicking elsewhere cancels
		markLayoutDirty();
	}
}

Vector2 UIKeyBindField::measureContent(Vector2 /*available*/) {
	const Vector2 text = ui::measureText(font(), getDisplayText(), theme().fontSize);
	return Vector2(std::max(text.x, 110.0f), text.y);
}

void UIKeyBindField::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	list.roundedRect(bounds, m_capturing ? t.activeColor : t.inputColor, t.controlRadius,
					 t.borderWidth,
					 m_capturing || isFocused() ? t.focusBorderColor : t.borderColor);
	ui::drawTextCentered(list, font(), getDisplayText(), t.fontSize, bounds,
						 m_key == kUnbound && !m_capturing ? t.mutedTextColor : t.textColor);
}

// ---------------------------------------------------------------- UIImage

UIImage::UIImage(std::shared_ptr<Texture> texture, UIImageFit fit)
	: m_texture(std::move(texture)), m_fit(fit) {}

UIImage& UIImage::setTexture(std::shared_ptr<Texture> texture) {
	m_texture = std::move(texture);
	markLayoutDirty();
	return *this;
}

UIImage& UIImage::setFit(UIImageFit fit) {
	m_fit = fit;
	markVisualDirty();
	return *this;
}

UIImage& UIImage::setTint(const Vector4& tint) {
	m_tint = tint;
	markVisualDirty();
	return *this;
}

UIImage& UIImage::setCornerRadius(float radius) {
	m_radius = std::max(radius, 0.0f);
	markVisualDirty();
	return *this;
}

Vector2 UIImage::measureContent(Vector2 /*available*/) {
	if (!m_texture) return Vector2(0.0f);
	return Vector2(static_cast<float>(m_texture->getWidth()),
				   static_cast<float>(m_texture->getHeight()));
}

UIImage::Placement UIImage::computePlacement() const {
	const UIRect box{getPadding().start(), contentSize()};
	Placement p{box, Vector2(0.0f), Vector2(1.0f)};
	if (!m_texture || m_texture->getWidth() == 0 || m_texture->getHeight() == 0 ||
		box.size.x <= 0.0f || box.size.y <= 0.0f) {
		return p;
	}
	const float imageAspect = static_cast<float>(m_texture->getWidth()) /
							  static_cast<float>(m_texture->getHeight());
	const float boxAspect = box.size.x / box.size.y;
	if (m_fit == UIImageFit::Contain) {
		// Shrink one axis of the rect so the whole image fits, centred.
		Vector2 size = box.size;
		if (imageAspect > boxAspect) {
			size.y = box.size.x / imageAspect;
		} else {
			size.x = box.size.y * imageAspect;
		}
		p.rect = {box.position + (box.size - size) * 0.5f, size};
	} else if (m_fit == UIImageFit::Cover) {
		// Keep the rect; crop the UV range symmetrically on one axis.
		if (imageAspect > boxAspect) {
			const float visible = boxAspect / imageAspect;
			p.uvMin.x = (1.0f - visible) * 0.5f;
			p.uvMax.x = 1.0f - p.uvMin.x;
		} else {
			const float visible = imageAspect / boxAspect;
			p.uvMin.y = (1.0f - visible) * 0.5f;
			p.uvMax.y = 1.0f - p.uvMin.y;
		}
	}
	return p;
}

void UIImage::draw(UIDrawList& list) {
	if (!m_texture) return;
	const Placement p = computePlacement();
	list.image(m_texture.get(), p.rect, p.uvMin, p.uvMax, m_tint, m_radius);
}

// ------------------------------------------------------------- UISplitter

UISplitter::UISplitter(UIAxis axis, float ratio)
	: m_axis(axis), m_ratio(std::clamp(ratio, 0.0f, 1.0f)), m_initialRatio(m_ratio) {
	m_interactive = true;
	m_first = add<UIObject>();
	m_second = add<UIObject>();
	m_first->setClipChildren(true);
	m_second->setClipChildren(true);
}

UISplitter& UISplitter::setRatio(float ratio) {
	const float clamped = std::clamp(ratio, 0.0f, 1.0f);
	if (clamped != m_ratio) {
		m_ratio = clamped;
		markLayoutDirty();
		if (m_onRatioChanged) m_onRatioChanged(m_ratio);
	}
	return *this;
}

UISplitter& UISplitter::setMinPaneSize(float pixels) {
	m_minPane = std::max(pixels, 0.0f);
	markLayoutDirty();
	return *this;
}

UISplitter& UISplitter::setOnRatioChanged(std::function<void(float)> onChanged) {
	m_onRatioChanged = std::move(onChanged);
	return *this;
}

float UISplitter::available() const {
	const Vector2 content = contentSize();
	return std::max((m_axis == UIAxis::Horizontal ? content.x : content.y) -
						kDividerThickness,
					0.0f);
}

float UISplitter::clampedRatio(float ratio) const {
	const float space = available();
	if (space <= 0.0f) return ratio;
	const float minRatio = std::min(m_minPane / space, 0.5f);
	return std::clamp(ratio, minRatio, 1.0f - minRatio);
}

UIRect UISplitter::dividerRect() const {
	const float first = available() * clampedRatio(m_ratio);
	const Vector2 origin = getPadding().start();
	if (m_axis == UIAxis::Horizontal) {
		return {origin + Vector2(first, 0.0f),
				Vector2(kDividerThickness, contentSize().y)};
	}
	return {origin + Vector2(0.0f, first), Vector2(contentSize().x, kDividerThickness)};
}

Vector2 UISplitter::measureContent(Vector2 available) {
	measureChildren(available);
	const Vector2 a = m_first->getDesiredSize(), b = m_second->getDesiredSize();
	return m_axis == UIAxis::Horizontal
			   ? Vector2(a.x + b.x + kDividerThickness, std::max(a.y, b.y))
			   : Vector2(std::max(a.x, b.x), a.y + b.y + kDividerThickness);
}

void UISplitter::arrangeChildren() {
	const UIRect divider = dividerRect();
	const Vector2 origin = getPadding().start();
	const Vector2 content = contentSize();
	if (m_axis == UIAxis::Horizontal) {
		const float firstWidth = divider.position.x - origin.x;
		m_first->arrange(origin, Vector2(firstWidth, content.y));
		m_second->arrange(Vector2(divider.max().x, origin.y),
						  Vector2(content.x - firstWidth - kDividerThickness, content.y));
	} else {
		const float firstHeight = divider.position.y - origin.y;
		m_first->arrange(origin, Vector2(content.x, firstHeight));
		m_second->arrange(Vector2(origin.x, divider.max().y),
						  Vector2(content.x, content.y - firstHeight - kDividerThickness));
	}
}

UICursor UISplitter::cursor() const {
	if (!m_dragging && !dividerRect().contains(m_lastLocalMouse)) {
		return UICursor::Arrow;
	}
	return m_axis == UIAxis::Horizontal ? UICursor::ResizeHorizontal
										: UICursor::ResizeVertical;
}

bool UISplitter::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	const Vector2 local = toLocal(event.position);
	const UIRect divider = dividerRect();
	if (!divider.contains(local)) return false;
	if (event.clickCount >= 2) {
		setRatio(m_initialRatio);
		return true;
	}
	m_dragging = true;
	m_dragOffset = m_axis == UIAxis::Horizontal ? local.x - divider.position.x
												: local.y - divider.position.y;
	return true;
}

bool UISplitter::onMouseMove(const UIMouseEvent& event) {
	m_lastLocalMouse = toLocal(event.position);
	if (!m_dragging) return false;
	const Vector2 local = toLocal(event.position) - getPadding().start();
	const float along = (m_axis == UIAxis::Horizontal ? local.x : local.y) - m_dragOffset;
	if (available() > 0.0f) setRatio(clampedRatio(along / available()));
	return true;
}

bool UISplitter::onMouseUp(const UIMouseEvent& /*event*/) {
	m_dragging = false;
	return true;
}

void UISplitter::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect divider = dividerRect();
	const Vector2 c = divider.position + divider.size * 0.5f;
	const Vector4 color = m_dragging || isHovered() ? t.accentColor : t.separatorColor;
	// Grip: three dots across the divider.
	for (int i = -1; i <= 1; i++) {
		const Vector2 offset = m_axis == UIAxis::Horizontal ? Vector2(0.0f, i * 6.0f)
															: Vector2(i * 6.0f, 0.0f);
		list.roundedRect({c + offset - Vector2(1.5f), Vector2(3.0f)}, color, 1.5f);
	}
}

}  // namespace bird
