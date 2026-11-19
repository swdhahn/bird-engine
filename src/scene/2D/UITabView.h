#ifndef BIRD_UITABVIEW_H
#define BIRD_UITABVIEW_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "UIButton.h"
#include "UIStack.h"

namespace bird {

/** Header button of a UITabView; drawn highlighted while selected. */
class UITabButton : public UIButton {
   public:
	explicit UITabButton(std::string text) : UIButton(std::move(text)) {}
	void setSelected(bool selected);
	bool isSelected() const { return m_selected; }
	void draw(UIDrawList& list) override;

   protected:
	Vector4 backgroundColor() const override;

   private:
	bool m_selected = false;
};

/**
 * A strip of tab buttons over a page area. Only the selected page is
 * visible (so only it is laid out, drawn and hit-tested). Pages stretch to
 * fill the page area.
 */
class UITabView : public UIStack {
   public:
	UITabView();

	/** Appends a page under a new tab and returns the page. */
	UIObject* addTab(std::string title, std::unique_ptr<UIObject> page);
	template <typename T, typename... Args>
	T* addTab(std::string title, Args&&... args) {
		return static_cast<T*>(addTab(
			std::move(title), std::make_unique<T>(std::forward<Args>(args)...)));
	}

	/** @throws std::out_of_range for an invalid index. */
	void selectTab(size_t index);
	size_t getSelectedTab() const { return m_selected; }
	size_t getTabCount() const { return m_pages.size(); }
	UITabButton* getTabButton(size_t index) const;
	UITabView& setOnTabChanged(std::function<void(size_t)> onTabChanged);

   private:
	UIStack* m_header;
	UIObject* m_pageArea;
	std::vector<UIObject*> m_pages;
	std::vector<UITabButton*> m_buttons;
	size_t m_selected = 0;
	std::function<void(size_t)> m_onTabChanged;
};

}  // namespace bird

#endif	// !BIRD_UITABVIEW_H
