#include "UIStack.h"

#include <algorithm>
#include <vector>

namespace bird {

namespace {

/** Component of a vector along the main (index 0) or cross axis. */
float mainOf(const Vector2& v, UIAxis axis) {
	return axis == UIAxis::Horizontal ? v.x : v.y;
}
float crossOf(const Vector2& v, UIAxis axis) {
	return axis == UIAxis::Horizontal ? v.y : v.x;
}
Vector2 fromAxes(float main, float cross, UIAxis axis) {
	return axis == UIAxis::Horizontal ? Vector2(main, cross)
									  : Vector2(cross, main);
}
UIAxis crossAxis(UIAxis axis) {
	return axis == UIAxis::Horizontal ? UIAxis::Vertical : UIAxis::Horizontal;
}
float marginStart(const UIInsets& m, UIAxis axis) {
	return axis == UIAxis::Horizontal ? m.left : m.top;
}

}  // namespace

UIStack::UIStack(UIAxis axis) : m_axis(axis) {}

UIStack& UIStack::setAxis(UIAxis axis) {
	m_axis = axis;
	markLayoutDirty();
	return *this;
}

UIStack& UIStack::setGap(float gap) {
	m_gap = std::max(gap, 0.0f);
	markLayoutDirty();
	return *this;
}

UIStack& UIStack::setJustify(UIJustify justify) {
	m_justify = justify;
	markLayoutDirty();
	return *this;
}

UIStack& UIStack::setAlign(UIAlign align) {
	m_align = align;
	markLayoutDirty();
	return *this;
}

float UIStack::getGap() const {
	return m_gap.value_or(theme().itemSpacing);
}

Vector2 UIStack::measureContent(Vector2 available) {
	measureChildren(available);
	float main = 0.0f, cross = 0.0f;
	int count = 0;
	for (auto& child : getChildren()) {
		if (!child->isVisible() ||
			child->getLayoutMode() == UILayoutMode::Absolute) {
			continue;
		}
		const Vector2 outer = child->getDesiredSize() + child->getMargin().total();
		main += mainOf(outer, m_axis);
		cross = std::max(cross, crossOf(outer, m_axis));
		count++;
	}
	if (count > 1) main += getGap() * static_cast<float>(count - 1);
	return fromAxes(main, cross, m_axis);
}

void UIStack::arrangeChildren() {
	std::vector<UIObject*> visible;
	for (auto& child : getChildren()) {
		if (!child->isVisible()) continue;
		if (child->getLayoutMode() == UILayoutMode::Absolute) {
			arrangeAnchored(*child, getPadding().start(), contentSize());
		} else {
			visible.push_back(child.get());
		}
	}
	if (visible.empty()) return;

	const Vector2 content = contentSize();
	const float contentMain = mainOf(content, m_axis);
	const float contentCross = crossOf(content, m_axis);
	const float gap = getGap();

	// Pass 1: space taken by non-fill children, margins and gaps.
	float used = gap * static_cast<float>(visible.size() - 1);
	float totalWeight = 0.0f;
	for (UIObject* child : visible) {
		used += mainOf(child->getMargin().total(), m_axis);
		const UISizeRule& rule = child->sizeRule(m_axis);
		if (rule.mode == UISizeMode::Fill) {
			totalWeight += std::max(rule.value, 0.0f);
		} else {
			used += mainOf(child->getDesiredSize(), m_axis);
		}
	}
	const float remaining = std::max(contentMain - used, 0.0f);

	// Free space is distributed by justify only when nothing fills it.
	float pen = 0.0f, extraGap = 0.0f;
	if (totalWeight <= 0.0f) {
		switch (m_justify) {
			case UIJustify::Start:
				break;
			case UIJustify::Center:
				pen = remaining * 0.5f;
				break;
			case UIJustify::End:
				pen = remaining;
				break;
			case UIJustify::SpaceBetween:
				if (visible.size() > 1) {
					extraGap = remaining / static_cast<float>(visible.size() - 1);
				}
				break;
		}
	}

	const UIAxis cross = crossAxis(m_axis);
	for (UIObject* child : visible) {
		const UISizeRule& mainRule = child->sizeRule(m_axis);
		const UISizeRule& crossRule = child->sizeRule(cross);
		const UIInsets& margin = child->getMargin();

		const float main =
			mainRule.mode == UISizeMode::Fill && totalWeight > 0.0f
				? mainRule.clamp(remaining * std::max(mainRule.value, 0.0f) /
								 totalWeight)
				: mainOf(child->getDesiredSize(), m_axis);

		const float crossSpace =
			std::max(contentCross - crossOf(margin.total(), m_axis), 0.0f);
		const float desiredCross = crossOf(child->getDesiredSize(), m_axis);
		float crossSize, crossPos;
		if (child->hasExplicitAnchors()) {
			// Explicit anchors replace align on the cross axis.
			const float aMin = crossOf(child->getAnchorMin(), m_axis);
			const float aMax = crossOf(child->getAnchorMax(), m_axis);
			if (aMin != aMax) {
				crossSize = crossRule.clamp((aMax - aMin) * crossSpace);
				crossPos = aMin * crossSpace;
			} else {
				crossSize = crossRule.mode == UISizeMode::Fill
								? crossRule.clamp(crossSpace)
								: desiredCross;
				crossPos = aMin * crossSpace -
						   crossOf(child->getPivot(), m_axis) * crossSize;
			}
		} else {
			const bool stretch = crossRule.mode == UISizeMode::Fill ||
								 (m_align == UIAlign::Stretch &&
								  crossRule.mode != UISizeMode::Fixed);
			crossSize = stretch ? crossRule.clamp(crossSpace) : desiredCross;
			crossPos = 0.0f;
			if (!stretch && m_align == UIAlign::Center) {
				crossPos = (crossSpace - crossSize) * 0.5f;
			} else if (!stretch && m_align == UIAlign::End) {
				crossPos = crossSpace - crossSize;
			}
		}

		pen += marginStart(margin, m_axis);
		// The offset is a visual nudge: it moves this child only.
		const Vector2 position =
			getPadding().start() +
			fromAxes(pen, marginStart(margin, cross) + crossPos, m_axis) +
			child->getOffset();
		child->arrange(position, fromAxes(main, crossSize, m_axis));
		pen += main + mainOf(margin.total(), m_axis) -
			   marginStart(margin, m_axis) + gap + extraGap;
	}
}

}  // namespace bird
