#ifndef BIRD_UIRADIOGROUP_H
#define BIRD_UIRADIOGROUP_H

#include <functional>
#include <string>
#include <vector>

#include "UIStack.h"

namespace bird {

class UIRadioGroup;

/** One option of a UIRadioGroup; created through UIRadioGroup::addOption. */
class UIRadioButton : public UIObject {
   public:
	UIRadioButton(UIRadioGroup& group, size_t index, std::string text);

	const std::string& getText() const { return m_text; }
	bool isSelected() const;

	/** Roving tab stop: only the selected (or first) option is a stop. */
	bool isFocusable() const override;
	UICursor cursor() const override { return UICursor::Hand; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	UIRadioGroup& m_group;
	size_t m_index;
	std::string m_text;
};

/**
 * Mutually exclusive options laid out as a stack (vertical by default).
 * Arrow keys move the selection (and focus) between options; Tab treats
 * the whole group as a single stop.
 */
class UIRadioGroup : public UIStack {
   public:
	static constexpr size_t kNone = static_cast<size_t>(-1);

	explicit UIRadioGroup(UIAxis axis = UIAxis::Vertical);

	/** Appends an option and returns its button. */
	UIRadioButton* addOption(std::string text);
	/**
	 * Selects an option (kNone clears); notifies onChanged on change.
	 * @throws std::out_of_range for an invalid index.
	 */
	UIRadioGroup& setSelected(size_t index);
	size_t getSelected() const { return m_selected; }
	size_t getOptionCount() const { return m_options.size(); }
	UIRadioButton* getOption(size_t index) const { return m_options.at(index); }
	UIRadioGroup& setOnChanged(std::function<void(size_t)> onChanged);

	/** Selects (wrapping) the option `delta` steps away and focuses it. */
	void moveSelection(int delta);

   private:
	std::vector<UIRadioButton*> m_options;
	size_t m_selected = kNone;
	std::function<void(size_t)> m_onChanged;
};

}  // namespace bird

#endif	// !BIRD_UIRADIOGROUP_H
