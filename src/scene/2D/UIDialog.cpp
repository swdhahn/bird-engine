#include "UIDialog.h"

#include "UIButton.h"
#include "UIDrawList.h"
#include "UILabel.h"
#include "UIManager.h"
#include "UIPanel.h"
#include "UIStack.h"

namespace bird {

UIDialog::UIDialog(std::string title, std::string message) {
	m_interactive = true;
	m_panel = add<UIPanel>();
	m_panel->setAnchor(UIAlignment::Center);
	m_panel->setPivot(UIAlignment::Center);
	m_panel->setPadding(UIInsets::all(theme().padding * 2.0f));
	UISizeRule width = UISizeRule::fit();
	width.min = 280.0f;
	m_panel->setWidth(width);

	auto* column = m_panel->add<UIStack>(UIAxis::Vertical);
	column->setGap(theme().padding * 1.5f);
	column->setAlign(UIAlign::Stretch);
	column->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	column->add<UILabel>(std::move(title))->setFontSize(theme().titleFontSize + 2.0f);
	m_body = column->add<UIStack>(UIAxis::Vertical);
	m_body->add<UILabel>(std::move(message))->setColor(theme().mutedTextColor);
	m_buttons = column->add<UIStack>(UIAxis::Horizontal);
	m_buttons->setJustify(UIJustify::End);
}

UIDialog& UIDialog::addButton(std::string text, std::function<void()> onClick) {
	m_buttons->add<UIButton>(std::move(text))
		->setOnClick([this, onClick = std::move(onClick)] {
			if (onClick) onClick();
			close();
		});
	return *this;
}

void UIDialog::close() {
	if (UIManager* manager = getManager();
		manager && manager->getTopModal() == this) {
		manager->closeModal(this);
	} else if (getParent()) {
		removeFromParent();
	}
}

void UIDialog::draw(UIDrawList& list) {
	list.rect({Vector2(0.0f), getSize()}, theme().backdropColor);
}

}  // namespace bird
