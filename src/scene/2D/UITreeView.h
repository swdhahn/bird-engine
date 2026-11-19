#ifndef BIRD_UITREEVIEW_H
#define BIRD_UITREEVIEW_H

#include <functional>
#include <string>
#include <vector>

#include "UIRowView.h"

namespace bird {

/** Plain tree data shown by UITreeView. */
struct UITreeNode {
	std::string text;
	std::vector<UITreeNode> children;
	bool expanded = false;

	static UITreeNode leaf(std::string text) {
		UITreeNode node;
		node.text = std::move(text);
		return node;
	}
	static UITreeNode branch(std::string text, std::vector<UITreeNode> children,
							 bool expanded = false) {
		UITreeNode node;
		node.text = std::move(text);
		node.children = std::move(children);
		node.expanded = expanded;
		return node;
	}
	bool hasChildren() const { return !children.empty(); }
};

/** Index path from the roots to a node, e.g. {0, 2} = 3rd child of root 0. */
using UITreePath = std::vector<size_t>;

/**
 * Virtualized, single-selection tree. Only expanded branches contribute
 * rows, and only visible rows are drawn.
 *
 * Mouse: click selects, clicking the disclosure triangle or double-clicking
 * a branch toggles it, double-clicking a leaf activates it. Keys: Up/Down/
 * Home/End/PageUp/PageDown move, Right expands (or enters the first child),
 * Left collapses (or moves to the parent), Enter activates, letters jump.
 *
 * The view owns its nodes. After editing them through getRoots(), call
 * refresh(); the selection is kept by path where it still exists.
 */
class UITreeView : public UIRowView {
   public:
	static constexpr float kIndent = 16.0f;

	explicit UITreeView(std::vector<UITreeNode> roots = {});

	UITreeView& setRoots(std::vector<UITreeNode> roots);
	std::vector<UITreeNode>& getRoots() { return m_roots; }
	/** Rebuilds the visible rows after the nodes were edited. */
	void refresh();

	/** @return the node at a path, or nullptr when it does not exist. */
	UITreeNode* nodeAt(const UITreePath& path);
	UITreeNode* getSelectedNode();
	const UITreePath& getSelectedPath() const { return m_selectedPath; }
	/** Selects a node, expanding its ancestors so it is visible. */
	bool selectPath(const UITreePath& path);
	void setExpanded(const UITreePath& path, bool expanded);
	void expandAll();
	void collapseAll();
	/** Path shown on a visible row. */
	const UITreePath& pathOfRow(size_t row) const { return m_rows.at(row).path; }
	size_t depthOfRow(size_t row) const { return m_rows.at(row).path.size() - 1; }

	UITreeView& setOnSelectionChanged(
		std::function<void(UITreeNode&, const UITreePath&)> onChanged);
	UITreeView& setOnActivate(
		std::function<void(UITreeNode&, const UITreePath&)> onActivate);

   protected:
	size_t rowCount() const override { return m_rows.size(); }
	std::string rowText(size_t row) const override { return m_rows[row].node->text; }
	void drawRow(UIDrawList& list, size_t row, const UIRect& rect) override;
	void onCursorMoved(size_t row, bool extend, bool toggle) override;
	bool onRowPressed(size_t row, Vector2 localInRow,
					  const UIMouseEvent& event) override;
	void onRowActivated(size_t row) override;
	bool onRowKey(const UIKeyEvent& event) override;

   private:
	struct Row {
		UITreeNode* node;
		UITreePath path;
	};
	void flatten(std::vector<UITreeNode>& nodes, UITreePath& path);
	size_t rowOfPath(const UITreePath& path) const;
	void toggleRow(size_t row);

	std::vector<UITreeNode> m_roots;
	std::vector<Row> m_rows;
	UITreePath m_selectedPath;
	std::function<void(UITreeNode&, const UITreePath&)> m_onSelectionChanged;
	std::function<void(UITreeNode&, const UITreePath&)> m_onActivate;
};

}  // namespace bird

#endif	// !BIRD_UITREEVIEW_H
