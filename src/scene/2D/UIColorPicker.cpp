#include "UIColorPicker.h"

#include <cmath>

#include "UIColorControls.h"
#include "UIDrawList.h"
#include "UILabel.h"
#include "UIManager.h"
#include "UINumberField.h"
#include "UIPanel.h"
#include "UITextBox.h"

namespace bird {

namespace {

constexpr float kWheelSize = 180.0f;
constexpr float kFieldWidth = 64.0f;

size_t index(UIColorPicker::Channel c) {
	return static_cast<size_t>(c);
}

/** Label + number field pair inside a row. */
UINumberField* addField(UIStack* row, const char* caption, double max) {
	row->add<UILabel>(caption)->setWidth(UISizeRule::fixed(14.0f));
	auto* field = row->add<UINumberField>(0.0, max, 0.0);
	field->setWidth(UISizeRule::fixed(kFieldWidth));
	return field;
}

}  // namespace

UIColorPicker::UIColorPicker(const Vector4& rgba, bool showAlpha)
	: UIStack(UIAxis::Vertical), m_alpha(1.0f) {
	setGap(10.0f);
	setAlign(UIAlign::Stretch);

	// --- Wheel, value bar, alpha bar, and the swatch/hex column.
	auto* top = add<UIStack>(UIAxis::Horizontal);
	top->setGap(10.0f);
	m_wheel = top->add<UIColorWheel>(kWheelSize);
	m_valueBar = top->add<UIColorSlider>(UIAxis::Vertical);
	m_valueBar->setHeight(UISizeRule::fixed(kWheelSize));
	m_valueBar->setTooltip("Value (brightness)");
	m_alphaBar = top->add<UIColorSlider>(UIAxis::Vertical);
	m_alphaBar->setHeight(UISizeRule::fixed(kWheelSize));
	m_alphaBar->setCheckered(true).setTooltip("Alpha (opacity)");
	m_alphaBar->setVisible(showAlpha);

	auto* side = top->add<UIStack>(UIAxis::Vertical);
	side->setGap(8.0f);
	side->setAlign(UIAlign::Stretch);
	side->setWidth(UISizeRule::fill());
	m_swatch = side->add<UIColorSwatch>();
	m_swatch->setTooltip("Click the left half to revert");
	side->add<UILabel>("Hex")->setColor(theme().mutedTextColor);
	m_hexField = side->add<UITextBox>();
	m_hexField->setMaxLength(9);
	m_hexField->setPlaceholder("#RRGGBB");
	auto* clipboard = side->add<UIStack>(UIAxis::Horizontal);
	clipboard->add<UIButton>("Copy")->setOnClick([this] { copyHex(); });
	clipboard->add<UIButton>("Paste")->setOnClick([this] { pasteHex(); });
	m_status = side->add<UILabel>("");
	m_status->setColor(theme().mutedTextColor);

	// --- Numeric HSV and RGBA rows.
	auto* hsvRow = add<UIStack>(UIAxis::Horizontal);
	hsvRow->setAlign(UIAlign::Center);
	m_fields[index(Channel::H)] = addField(hsvRow, "H", 360.0);
	m_fields[index(Channel::S)] = addField(hsvRow, "S", 100.0);
	m_fields[index(Channel::V)] = addField(hsvRow, "V", 100.0);
	auto* rgbRow = add<UIStack>(UIAxis::Horizontal);
	rgbRow->setAlign(UIAlign::Center);
	m_fields[index(Channel::R)] = addField(rgbRow, "R", 255.0);
	m_fields[index(Channel::G)] = addField(rgbRow, "G", 255.0);
	m_fields[index(Channel::B)] = addField(rgbRow, "B", 255.0);
	m_fields[index(Channel::A)] = addField(rgbRow, "A", 100.0);
	m_fields[index(Channel::A)]->setVisible(showAlpha);

	// --- R, G, B gradient sliders.
	const char* names[3] = {"R", "G", "B"};
	for (int c = 0; c < 3; c++) {
		auto* row = add<UIStack>(UIAxis::Horizontal);
		row->setAlign(UIAlign::Center);
		row->add<UILabel>(names[c])->setWidth(UISizeRule::fixed(14.0f));
		m_rgbSliders[c] = row->add<UIColorSlider>(UIAxis::Horizontal);
		m_rgbSliders[c]->setWidth(UISizeRule::fill());
	}

	// --- Wiring: every control edits the shared colour.
	m_wheel->setOnChanged([this](float h, float s) {
		apply({h, s, m_hsv.v}, m_alpha, m_wheel);
	});
	m_valueBar->setOnChanged([this](float v) {
		apply({m_hsv.h, m_hsv.s, v}, m_alpha, m_valueBar);
	});
	m_alphaBar->setOnChanged([this](float a) { apply(m_hsv, a, m_alphaBar); });
	for (int c = 0; c < 3; c++) {
		m_rgbSliders[c]->setOnChanged([this, c](float value) {
			Vector3 next = rgb();
			next[c] = value;
			applyRgb(next, m_rgbSliders[c]);
		});
	}
	auto onField = [this](Channel channel) {
		return [this, channel](double value) {
			if (m_syncing) return;
			const float v = static_cast<float>(value);
			UIObject* source = m_fields[index(channel)];
			switch (channel) {
				case Channel::H: apply({v, m_hsv.s, m_hsv.v}, m_alpha, source); break;
				case Channel::S: apply({m_hsv.h, v / 100.0f, m_hsv.v}, m_alpha, source); break;
				case Channel::V: apply({m_hsv.h, m_hsv.s, v / 100.0f}, m_alpha, source); break;
				case Channel::A: apply(m_hsv, v / 100.0f, source); break;
				default: {
					Vector3 next = rgb();
					next[static_cast<int>(channel) - static_cast<int>(Channel::R)] =
						v / 255.0f;
					applyRgb(next, source);
				}
			}
		};
	};
	for (size_t c = 0; c < m_fields.size(); c++) {
		m_fields[c]->setOnValueChanged(onField(static_cast<Channel>(c)));
	}
	// Hex applies live once it parses; Enter rewrites it canonically.
	m_hexField->setOnChanged([this](const std::string& text) {
		if (m_syncing) return;
		if (const auto parsed = color::parseHex(text)) {
			apply(color::rgbToHsv(Vector3(*parsed), m_hsv.h), parsed->a, m_hexField);
		}
	});
	m_hexField->setOnSubmit([this](const std::string& text) {
		if (!setHex(text)) setStatus("Not a hex colour: " + text);
		sync(nullptr);	// normalize the field either way
	});
	m_swatch->setOnRevert([this](const Vector4& original) {
		apply(color::rgbToHsv(Vector3(original), m_hsv.h), original.a, nullptr);
	});

	setColor(rgba);
}

Vector3 UIColorPicker::rgb() const {
	return color::hsvToRgb(m_hsv);
}

Vector4 UIColorPicker::getColor() const {
	return Vector4(rgb(), m_alpha);
}

std::string UIColorPicker::getHex() const {
	return color::toHex(getColor());
}

void UIColorPicker::setColor(const Vector4& rgba) {
	m_hsv = color::rgbToHsv(Vector3(rgba), m_hsv.h);
	m_alpha = std::clamp(rgba.a, 0.0f, 1.0f);
	m_swatch->setOriginal(getColor());
	sync(nullptr);
}

bool UIColorPicker::setHex(const std::string& hex) {
	const auto parsed = color::parseHex(hex);
	if (!parsed) return false;
	apply(color::rgbToHsv(Vector3(*parsed), m_hsv.h), parsed->a, nullptr);
	return true;
}

UIColorPicker& UIColorPicker::setOnChanged(
	std::function<void(const Vector4&)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

UIColorSlider* UIColorPicker::getChannelSlider(Channel channel) const {
	const int c = static_cast<int>(channel) - static_cast<int>(Channel::R);
	return (c >= 0 && c < 3) ? m_rgbSliders[static_cast<size_t>(c)] : nullptr;
}

UINumberField* UIColorPicker::getField(Channel channel) const {
	return m_fields[index(channel)];
}

const std::string& UIColorPicker::getStatus() const {
	return m_status->getText();
}

void UIColorPicker::setStatus(const std::string& text) {
	m_status->setText(text);
}

void UIColorPicker::copyHex() {
	const std::string hex = getHex();
	if (UIPlatform* p = platform()) {
		p->setClipboardText(hex);
		setStatus("Copied " + hex);
	}
}

bool UIColorPicker::pasteHex() {
	UIPlatform* p = platform();
	if (!p) return false;
	const std::string text = p->getClipboardText();
	const bool ok = setHex(text);
	setStatus(ok ? "Pasted " + getHex() : "Clipboard has no hex colour");
	return ok;
}

void UIColorPicker::apply(const color::HSV& hsv, float alpha,
						  const UIObject* source) {
	if (m_syncing) return;	// a control echoing a sync, not the user
	m_hsv = {std::fmod(std::fmod(hsv.h, 360.0f) + 360.0f, 360.0f),
			 std::clamp(hsv.s, 0.0f, 1.0f), std::clamp(hsv.v, 0.0f, 1.0f)};
	m_alpha = std::clamp(alpha, 0.0f, 1.0f);
	sync(source);
	if (m_onChanged) m_onChanged(getColor());
}

void UIColorPicker::applyRgb(const Vector3& rgbValue, const UIObject* source) {
	apply(color::rgbToHsv(rgbValue, m_hsv.h), m_alpha, source);
}

void UIColorPicker::sync(const UIObject* source) {
	m_syncing = true;
	const Vector3 c = rgb();
	const Vector4 rgba = getColor();
	if (source != m_wheel) m_wheel->setHueSaturation(m_hsv.h, m_hsv.s);
	if (source != m_valueBar) m_valueBar->setValue(m_hsv.v);
	if (source != m_alphaBar) m_alphaBar->setValue(m_alpha);
	m_valueBar->setGradient(Vector4(0, 0, 0, 1),
							Vector4(color::hsvToRgb({m_hsv.h, m_hsv.s, 1.0f}), 1.0f));
	m_alphaBar->setGradient(Vector4(c, 0.0f), Vector4(c, 1.0f));
	for (int i = 0; i < 3; i++) {
		Vector3 low = c, high = c;
		low[i] = 0.0f;
		high[i] = 1.0f;
		m_rgbSliders[static_cast<size_t>(i)]->setGradient(Vector4(low, 1.0f),
														  Vector4(high, 1.0f));
		if (source != m_rgbSliders[static_cast<size_t>(i)]) {
			m_rgbSliders[static_cast<size_t>(i)]->setValue(c[i]);
		}
	}
	const double values[7] = {m_hsv.h,		   m_hsv.s * 100.0, m_hsv.v * 100.0,
							  c.r * 255.0,	   c.g * 255.0,		c.b * 255.0,
							  m_alpha * 100.0};
	for (size_t i = 0; i < m_fields.size(); i++) {
		if (source != m_fields[i]) m_fields[i]->setValue(values[i]);
	}
	if (source != m_hexField) m_hexField->setText(getHex());
	m_swatch->setColor(rgba);
	m_syncing = false;
}

// --------------------------------------------------------- UIColorButton

UIColorButton::UIColorButton(const Vector4& rgba) : UIButton(""), m_color(rgba) {
	setOnClick([this] { openPicker(); });
}

void UIColorButton::setColor(const Vector4& rgba) {
	m_color = rgba;
	if (m_picker) m_picker->setColor(rgba);
	markVisualDirty();
}

UIColorButton& UIColorButton::setOnColorChanged(
	std::function<void(const Vector4&)> onChanged) {
	m_onColorChanged = std::move(onChanged);
	return *this;
}

Vector2 UIColorButton::measureContent(Vector2 /*available*/) {
	return Vector2(44.0f, 16.0f);
}

void UIColorButton::openPicker() {
	UIManager* manager = getManager();
	if (!manager || m_popup) return;
	auto panel = std::make_unique<UIPanel>();
	panel->setShadow(true);
	panel->setPadding(UIInsets::all(12.0f));
	auto* picker = panel->add<UIColorPicker>(m_color);
	picker->setOnChanged([this](const Vector4& rgba) {
		m_color = rgba;
		markVisualDirty();
		if (m_onColorChanged) m_onColorChanged(rgba);
	});
	m_picker = picker;
	manager->layoutNow();
	m_popup = manager->openPopup(this, std::move(panel), getScreenRect());
}

void UIColorButton::onPopupClosed(UIObject* popup) {
	if (popup == m_popup) {
		m_popup = nullptr;
		m_picker = nullptr;
	}
}

void UIColorButton::draw(UIDrawList& list) {
	UIButton::draw(list);
	const UIRect inner{Vector2(6.0f), getSize() - Vector2(12.0f)};
	ui::drawTransparencyChecker(list, inner);
	list.roundedRect(inner, m_color, 3.0f, 1.0f, theme().borderColor);
}

}  // namespace bird
