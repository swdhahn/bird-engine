#include "UITreeView.h"

#include <algorithm>

#include "UITextUtil.h"

namespace bird {

namespace {
void setAllExpanded(std::vector<UITreeNode>& nodes, bool expanded) {
	for (UITreeNode& node : nodes) {
		if (node.hasChildren()) node.expanded = expanded;
		setAllExpanded(node.children, expanded);
	}
}
}  // namespace

UITreeView::UITreeView(std::vector<UITreeNode> roots) {
	setRoots(std::move(roots));
}

UITreeView& UITreeView::setRoots(std::vector<UITreeNode> roots) {
	m_roots = std::move(roots);
	m_selectedPath.clear();
	refresh();
	setScrollOffset(0.0f);
	return *this;
}

void UITreeView::flatten(std::vector<UITreeNode>& nodes, UITreePath& path) {
	for (size_t i = 0; i < nodes.size(); i++) {
		path.push_back(i);
		m_rows.push_back({&nodes[i], path});
		if (nodes[i].expanded) flatten(nodes[i].children, path);
		path.pop_back();
	}
}

void UITreeView::refresh() {
	m_rows.clear();
	UITreePath path;
	flatten(m_roots, path);	 // O(visible nodes)
	// Keep the selection by path; if it vanished, fall back to the deepest
	// ancestor that still exists.
	while (!m_selectedPath.empty() && !nodeAt(m_selectedPath)) {
		m_selectedPath.pop_back();
	}
	const size_t row = rowOfPath(m_selectedPath);
	setCursorSilently(row);
	markLayoutDirty();
}

UITreeNode* UITreeView::nodeAt(const UITreePath& path) {
	std::vector<UITreeNode>* level = &m_roots;
	UITreeNode* node = nullptr;
	for (size_t index : path) {
		if (index >= level->size()) return nullptr;
		node = &(*level)[index];
		level = &node->children;
	}
	return node;
}

size_t UITreeView::rowOfPath(const UITreePath& path) const {
	if (path.empty()) return kNone;
	for (size_t row = 0; row < m_rows.size(); row++) {
		if (m_rows[row].path == path) return row;
	}
	return kNone;
}

UITreeNode* UITreeView::getSelectedNode() {
	return m_selectedPath.empty() ? nullptr : nodeAt(m_selectedPath);
}

bool UITreeView::selectPath(const UITreePath& path) {
	if (!nodeAt(path)) return false;
	// Expand every ancestor so the node gets a row.
	for (size_t depth = 1; depth < path.size(); depth++) {
		nodeAt(UITreePath(path.begin(), path.begin() + static_cast<long>(depth)))
			->expanded = true;
	}
	refresh();
	moveCursor(rowOfPath(path), false, false);
	return true;
}

void UITreeView::setExpanded(const UITreePath& path, bool expanded) {
	UITreeNode* node = nodeAt(path);
	if (!node || !node->hasChildren() || node->expanded == expanded) return;
	node->expanded = expanded;
	// Collapsing a branch that holds the selection moves it to the branch.
	if (!expanded && m_selectedPath.size() > path.size() &&
		std::equal(path.begin(), path.end(), m_selectedPath.begin())) {
		m_selectedPath = path;
	}
	refresh();
}

void UITreeView::expandAll() {
	setAllExpanded(m_roots, true);
	refresh();
}

void UITreeView::collapseAll() {
	setAllExpanded(m_roots, false);
	if (!m_selectedPath.empty()) m_selectedPath.resize(1);
	refresh();
}

void UITreeView::toggleRow(size_t row) {
	const Row& r = m_rows.at(row);
	setExpanded(UITreePath(r.path), !r.node->expanded);
}

UITreeView& UITreeView::setOnSelectionChanged(
	std::function<void(UITreeNode&, const UITreePath&)> onChanged) {
	m_onSelectionChanged = std::move(onChanged);
	return *this;
}

UITreeView& UITreeView::setOnActivate(
	std::function<void(UITreeNode&, const UITreePath&)> onActivate) {
	m_onActivate = std::move(onActivate);
	return *this;
}

void UITreeView::onCursorMoved(size_t row, bool /*extend*/, bool /*toggle*/) {
	if (m_rows[row].path == m_selectedPath) return;
	m_selectedPath = m_rows[row].path;
	markVisualDirty();
	if (m_onSelectionChanged) m_onSelectionChanged(*m_rows[row].node, m_selectedPath);
}

bool UITreeView::onRowPressed(size_t row, Vector2 localInRow,
							  const UIMouseEvent& event) {
	const float indent = static_cast<float>(depthOfRow(row)) * kIndent;
	const bool onTriangle = m_rows[row].node->hasChildren() &&
							localInRow.x >= indent && localInRow.x < indent + kIndent;
	if (onTriangle) {
		moveCursor(row, false, false);
		toggleRow(rowOfPath(m_selectedPath));
		return true;
	}
	if (event.clickCount == 2 && m_rows[row].node->hasChildren()) {
		moveCursor(row, false, false);
		toggleRow(row);
		return true;
	}
	return false;  // default: select, and double click activates leaves
}

void UITreeView::onRowActivated(size_t row) {
	if (m_onActivate) m_onActivate(*m_rows[row].node, m_rows[row].path);
}

bool UITreeView::onRowKey(const UIKeyEvent& event) {
	const size_t row = getCursorRow();
	if (row == kNone || row >= m_rows.size()) return false;
	const Row current = m_rows[row];
	if (event.key == UIKey::Right) {
		if (current.node->hasChildren() && !current.node->expanded) {
			toggleRow(row);
		} else if (current.node->hasChildren()) {
			moveCursor(row + 1, false, false);	// first child
		}
		return true;
	}
	if (event.key == UIKey::Left) {
		if (current.node->hasChildren() && current.node->expanded) {
			toggleRow(row);
		} else if (current.path.size() > 1) {
			moveCursor(rowOfPath(UITreePath(current.path.begin(),
											current.path.end() - 1)),
					   false, false);
		}
		return true;
	}
	return false;
}

void UITreeView::drawRow(UIDrawList& list, size_t row, const UIRect& rect) {
	const UITheme& t = theme();
	const Row& r = m_rows[row];
	if (r.path == m_selectedPath) {
		list.roundedRect(rect, isFocused() ? t.menuHighlightColor : t.activeColor,
						 t.controlRadius);
	}
	const float x = rect.position.x + static_cast<float>(depthOfRow(row)) * kIndent;
	const Vector2 c(x + kIndent * 0.5f, rect.position.y + rect.size.y * 0.5f);
	if (r.node->hasChildren()) {
		if (r.node->expanded) {	 // pointing down
			list.triangle(c + Vector2(-4.0f, -2.5f), c + Vector2(0.0f, 3.0f),
						  c + Vector2(4.0f, -2.5f), t.mutedTextColor);
		} else {  // pointing right
			list.triangle(c + Vector2(-2.5f, -4.0f), c + Vector2(-2.5f, 4.0f),
						  c + Vector2(3.0f, 0.0f), t.mutedTextColor);
		}
	}
	ui::drawTextCenteredY(list, font(), r.node->text, t.fontSize,
						  x + kIndent + 2.0f, rect, t.textColor);
}

}  // namespace bird
