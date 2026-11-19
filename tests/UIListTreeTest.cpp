#include <gtest/gtest.h>

#include "UIHarness.h"
#include "scene/2D/UIExpander.h"
#include "scene/2D/UIListView.h"
#include "scene/2D/UITreeView.h"

using namespace bird;
using bird::test::UIHarness;

namespace {

constexpr float kPad = 3.0f;	 // UIRowView padding
constexpr float kRow = 26.0f;	 // theme rowHeight

/** Screen point inside a visible row (x is local to the view). */
Vector2 rowPoint(UIHarness& h, UIRowView* view, size_t row, float x = 40.0f) {
	return h.atLocal(view, {x, kPad + kRow * static_cast<float>(row) + kRow * 0.5f});
}

std::vector<size_t> sel(std::initializer_list<size_t> v) { return v; }

}  // namespace

// ---------------------------------------------------------------- list

TEST(UIListView, SingleSelectionByClickAndArrows) {
	UIHarness h;
	auto* list = h.root()->add<UIListView>(
		std::vector<std::string>{"a", "b", "c", "d", "e"});
	int changes = 0;
	list->setOnSelectionChanged([&] { changes++; });
	h.click(rowPoint(h, list, 2));
	EXPECT_EQ(list->getSelection(), sel({2}));
	h.press(UIKey::Down);
	EXPECT_EQ(list->getSelectedIndex(), 3u);
	h.press(UIKey::End);
	EXPECT_EQ(list->getSelectedIndex(), 4u);
	EXPECT_EQ(changes, 3);
}

TEST(UIListView, MultipleSelectionWithModifiers) {
	UIHarness h;
	auto* list = h.root()->add<UIListView>(
		std::vector<std::string>{"a", "b", "c", "d", "e"});
	list->setSelectionMode(UISelectionMode::Multiple);
	h.click(rowPoint(h, list, 1));
	h.click(rowPoint(h, list, 3), UI_MOD_CONTROL);
	EXPECT_EQ(list->getSelection(), sel({1, 3}));
	h.click(rowPoint(h, list, 4), UI_MOD_SHIFT);  // range from anchor 3
	EXPECT_EQ(list->getSelection(), sel({3, 4}));
	h.press(UIKey::Up, UI_MOD_SHIFT);
	EXPECT_EQ(list->getSelection(), sel({3}));
	h.press(UIKey::A, UI_MOD_CONTROL);
	EXPECT_EQ(list->getSelection().size(), 5u);
	h.press(UIKey::Space);	// toggles the cursor row (3) off
	EXPECT_FALSE(list->isSelected(3));
}

TEST(UIListView, VirtualizesAMillionRows) {
	UIHarness h;
	size_t calls = 0;
	auto* list = h.root()->add<UIListView>();
	list->setItemProvider(1'000'000, [&](size_t i) {
		calls++;
		return "Item " + std::to_string(i);
	});
	h.frame();
	EXPECT_LT(calls, 50u);	// only visible rows were asked for
	EXPECT_LT(h.ui.getDrawList().vertices().size(), 2000u);

	h.ui.setFocus(list);
	h.press(UIKey::End);
	h.frame();
	EXPECT_EQ(list->getCursorRow(), 999'999u);
	EXPECT_FLOAT_EQ(list->getScrollOffset(), list->getMaxScroll());
	EXPECT_LT(h.ui.getDrawList().vertices().size(), 2000u);
}

TEST(UIListView, DoubleClickAndEnterActivate) {
	UIHarness h;
	auto* list = h.root()->add<UIListView>(std::vector<std::string>{"a", "b"});
	std::vector<size_t> activated;
	list->setOnActivate([&](size_t i) { activated.push_back(i); });
	const Vector2 p = rowPoint(h, list, 1);
	h.click(p);
	h.click(p);	 // second click within the double-click window
	h.press(UIKey::Up);
	h.press(UIKey::Enter);
	EXPECT_EQ(activated, sel({1, 0}));
}

TEST(UIListView, TypeAheadAndWheel) {
	UIHarness h;
	std::vector<std::string> items = {"apple", "banana", "blueberry"};
	for (int i = 0; i < 30; i++) items.push_back("cherry " + std::to_string(i));
	auto* list = h.root()->add<UIListView>(items);
	h.ui.setFocus(list);
	h.type("b");
	EXPECT_EQ(list->getSelectedIndex(), 1u);
	h.type("b");
	EXPECT_EQ(list->getSelectedIndex(), 2u);
	h.ui.scroll(h.at(list), Vector2(0, -1));
	EXPECT_FLOAT_EQ(list->getScrollOffset(), 3 * kRow);
}

// ---------------------------------------------------------------- tree

namespace {

std::vector<UITreeNode> sceneTree() {
	return {UITreeNode::branch("Scene",
							   {UITreeNode::leaf("Camera"),
								UITreeNode::branch("Props", {UITreeNode::leaf("Barrel"),
															 UITreeNode::leaf("Crate")}),
								UITreeNode::leaf("Light")},
							   true),
			UITreeNode::leaf("Sky")};
}

}  // namespace

TEST(UITreeView, FlattensOnlyExpandedBranches) {
	UIHarness h;
	auto* tree = h.root()->add<UITreeView>(sceneTree());
	EXPECT_EQ(tree->getCursorRow(), UIRowView::kNone);
	h.ui.layoutNow();
	EXPECT_EQ(tree->pathOfRow(2), (UITreePath{0, 1}));	// Props
	EXPECT_EQ(tree->depthOfRow(2), 1u);
	tree->expandAll();
	EXPECT_EQ(tree->pathOfRow(4), (UITreePath{0, 1, 1}));  // Crate
	tree->collapseAll();
	EXPECT_EQ(tree->pathOfRow(1), (UITreePath{1}));	 // Sky
}

TEST(UITreeView, RightAndLeftExpandEnterAndClimb) {
	UIHarness h;
	auto* tree = h.root()->add<UITreeView>(sceneTree());
	h.ui.setFocus(tree);
	tree->selectPath({0, 1});  // Props
	h.press(UIKey::Right);	   // expand
	EXPECT_TRUE(tree->nodeAt({0, 1})->expanded);
	h.press(UIKey::Right);	// into the first child
	EXPECT_EQ(tree->getSelectedNode()->text, "Barrel");
	h.press(UIKey::Left);  // up to the parent
	EXPECT_EQ(tree->getSelectedPath(), (UITreePath{0, 1}));
	h.press(UIKey::Left);  // collapse
	EXPECT_FALSE(tree->nodeAt({0, 1})->expanded);
}

TEST(UITreeView, ClickingTheTriangleToggles) {
	UIHarness h;
	auto* tree = h.root()->add<UITreeView>(sceneTree());
	// Props is row 2 at depth 1: its triangle spans local x 19..35.
	h.click(rowPoint(h, tree, 2, kPad + UITreeView::kIndent + 8.0f));
	EXPECT_TRUE(tree->nodeAt({0, 1})->expanded);
	EXPECT_EQ(tree->getSelectedPath(), (UITreePath{0, 1}));
}

TEST(UITreeView, CollapsingAnAncestorMovesTheSelectionUp) {
	UIHarness h;
	auto* tree = h.root()->add<UITreeView>(sceneTree());
	tree->selectPath({0, 1, 0});  // Barrel (expands Props)
	tree->setExpanded({0}, false);
	EXPECT_EQ(tree->getSelectedPath(), (UITreePath{0}));
	EXPECT_EQ(tree->getCursorRow(), 0u);
}

TEST(UITreeView, SelectionSurvivesEditsWherePossible) {
	UIHarness h;
	auto* tree = h.root()->add<UITreeView>(sceneTree());
	tree->selectPath({1});	// Sky
	tree->getRoots().pop_back();
	tree->refresh();
	EXPECT_EQ(tree->getSelectedNode(), nullptr);  // gone, no dangling pointer
	EXPECT_TRUE(tree->getSelectedPath().empty());
}

TEST(UITreeView, ActivationAndTypeAhead) {
	UIHarness h;
	auto* tree = h.root()->add<UITreeView>(sceneTree());
	std::string activated;
	tree->setOnActivate([&](UITreeNode& node, const UITreePath&) {
		activated = node.text;
	});
	h.ui.setFocus(tree);
	h.type("l");
	EXPECT_EQ(tree->getSelectedNode()->text, "Light");
	h.press(UIKey::Enter);
	EXPECT_EQ(activated, "Light");
	const Vector2 camera = rowPoint(h, tree, 1, 80.0f);
	h.click(camera);
	h.click(camera);
	EXPECT_EQ(activated, "Camera");
}

TEST(UITreeView, SelectPathRevealsCollapsedNodes) {
	UIHarness h;
	auto* tree = h.root()->add<UITreeView>(sceneTree());
	tree->collapseAll();
	ASSERT_TRUE(tree->selectPath({0, 1, 1}));
	EXPECT_EQ(tree->getSelectedNode()->text, "Crate");
	EXPECT_TRUE(tree->nodeAt({0})->expanded);
	EXPECT_FALSE(tree->selectPath({5}));
}

// ------------------------------------------------------------ expander

TEST(UIExpander, TogglingHidesContentAndShrinks) {
	UIHarness h;
	auto* expander = h.root()->add<UIExpander>("Advanced");
	expander->getContent()->add<UIObject>()->setSize({100, 50});
	std::vector<bool> toggles;
	expander->setOnToggled([&](bool open) { toggles.push_back(open); });
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(expander->getSize().y, 26 + 50 + 12);
	h.click(expander->getHeader());
	h.ui.layoutNow();
	EXPECT_FALSE(expander->isExpanded());
	EXPECT_FLOAT_EQ(expander->getSize().y, 26);
	h.press(UIKey::Space);
	EXPECT_TRUE(expander->isExpanded());
	EXPECT_EQ(toggles, (std::vector<bool>{false, true}));
}

TEST(UIListView, SelectionBeforeLayoutScrollsIntoViewAfterIt) {
	UIHarness h;
	std::vector<std::string> items(100, "row");
	auto* list = h.root()->add<UIListView>(items);
	list->select(90);  // not laid out yet
	h.ui.layoutNow();
	EXPECT_GT(list->getScrollOffset(), 0.0f);
	const UIRect row = list->rowRect(90);
	EXPECT_GE(row.position.y, 0.0f);
	EXPECT_LE(row.max().y, list->getSize().y);
}
