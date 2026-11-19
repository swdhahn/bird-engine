# Tutorial 3: Building UI Layouts & Widgets

Bird Engine features an immediate-to-retained 2D UI framework managed by `bird::UIManager`.

In this tutorial, you will learn how to:
1. Access the `UIManager`.
2. Use flexbox-style `UIStack` layouts.
3. Configure layout anchors, pivots, and sizing rules.
4. Add interactive controls (`UIButton`, `UISlider`, `UICheckBox`, `UITextBox`).
5. Wire callbacks and display modal dialogs (`UIDialog`).
6. Show data in lists and trees (`UIListView`, `UITreeView`, `UIExpander`).
7. Add a colour picker and the rich inputs (`UIColorPicker`, `UIRangeSlider`, `UIKeyBindField`, `UIImage`, `UISplitter`).

---

## 1. Accessing the UI Manager

The `UIManager` instance is created during `Application::init()` and is accessible through the global application pointer:

```cpp
bird::UIManager* ui = bird::APPLICATION->getUIManager();
bird::UIObject* root = ui->getRoot();
```

---

## 2. Layout Basics: Stacks, Anchors, and Sizing Rules

Bird Engine provides two primary layout paradigms:
- **Anchors & Pivots**: Positions elements relative to parent bounds (e.g. pinned to TopLeft, Center, or BottomRight).
- **`UIStack`**: Linear flexbox layout arranging children horizontally or vertically with automated padding and gaps.

### Size Rules
- `UISizeRule::fixed(pixels)`: Hard-coded dimension (e.g. `fixed(200.0f)`).
- `UISizeRule::fill(weight)`: Shares remaining space along the axis proportionally (e.g. weight `1.0f`).
- `UISizeRule::fit()`: Shrinks to fit content (e.g. text size).

---

## 3. Creating a Floating Window

Let's create a desktop window with tabs:

```cpp
#include <scene/2D/UIWindow.h>
#include <scene/2D/UITabView.h>
#include <scene/2D/UIStack.h>
#include <scene/2D/UILabel.h>
#include <scene/2D/UIButton.h>
#include <scene/2D/UISlider.h>
#include <scene/2D/UICheckBox.h>
#include <scene/2D/UITextBox.h>
#include <scene/2D/UIDialog.h>

void createSettingsWindow(bird::UIManager* ui) {
    using namespace bird;

    // 1. Add a window to the root
    auto* window = ui->getRoot()->add<UIWindow>("Game Settings", Vector2(400, 350));
    window->setOffset(Vector2(50.0f, 50.0f));

    // 2. Add a TabView inside the window body
    auto* tabs = window->getContent()->add<UITabView>();
    tabs->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);

    // 3. Create "Audio" tab
    auto* audioPage = tabs->addTab<UIStack>("Audio", UIAxis::Vertical);
    audioPage->setPadding(UIInsets::all(12.0f));
    audioPage->setGap(10.0f);
    audioPage->setAlign(UIAlign::Stretch);

    // Master volume label & slider
    audioPage->add<UILabel>("Master Volume");
    auto* slider = audioPage->add<UISlider>(0.0f, 1.0f, 0.75f);
    slider->setWidth(UISizeRule::fill());
    slider->setOnChanged([](float val) {
        std::cout << "Volume changed to: " << (val * 100.0f) << "%" << std::endl;
    });

    // Mute checkbox
    audioPage->add<UICheckBox>("Mute Audio", false)->setOnChanged([](bool muted) {
        std::cout << (muted ? "Audio muted" : "Audio active") << std::endl;
    });

    // 4. Create "Profile" tab
    auto* profilePage = tabs->addTab<UIStack>("Profile", UIAxis::Vertical);
    profilePage->setPadding(UIInsets::all(12.0f));
    profilePage->setGap(10.0f);
    profilePage->setAlign(UIAlign::Stretch);

    auto* nameInput = profilePage->add<UITextBox>();
    nameInput->setPlaceholder("Enter player name...");
    nameInput->setWidth(UISizeRule::fill());
    nameInput->setOnSubmit([](const std::string& name) {
        std::cout << "Player name submitted: " << name << std::endl;
    });

    // Discard button opening a modal dialog
    auto* discardBtn = profilePage->add<UIButton>("Reset Profile");
    discardBtn->setOnClick([ui]() {
        auto dialog = std::make_unique<UIDialog>(
            "Reset Profile?", "Are you sure you want to revert all changes?");
        dialog->addButton("Cancel", [] {});
        dialog->addButton("Reset", [] {
            std::cout << "Profile reset confirmed!" << std::endl;
        });
        ui->pushModal(std::move(dialog));
    });
}
```

---

## 4. Corner HUD Overlay

To position a HUD element (like a score counter or title) relative to the screen corners:

```cpp
// Anchored to the top-left
auto* title = ui->getRoot()->add<bird::UILabel>("Bird Engine v0.1");
title->setFontSize(32.0f);
title->setOffset(bird::Vector2(20.0f, 20.0f));

// Anchored to the bottom-right corner (follows window resize)
auto* helpBtn = ui->getRoot()->add<bird::UIButton>("Help");
helpBtn->setAnchor(bird::UIAlignment::BottomRight);
helpBtn->setPivot(bird::UIAlignment::BottomRight);
helpBtn->setOffset(bird::Vector2(-20.0f, -20.0f));
```

---

## 5. Lists, Trees, and Collapsible Sections

A settings page listing assets and a scene hierarchy. Both views are virtualized, so the 10,000-row list costs no more to draw than a 10-row one:

```cpp
#include <scene/2D/UIExpander.h>
#include <scene/2D/UIListView.h>
#include <scene/2D/UITreeView.h>

void buildAssetBrowser(bird::UIObject* page) {
    auto* column = page->add<bird::UIStack>(bird::UIAxis::Vertical);
    column->setAnchors(bird::UIAlignment::TopLeft, bird::UIAlignment::BottomRight);
    column->setAlign(bird::UIAlign::Stretch);

    auto* views = column->add<bird::UIStack>(bird::UIAxis::Horizontal);
    views->setAlign(bird::UIAlign::Stretch);

    // Generated rows: the provider is only called for rows on screen.
    auto* list = views->add<bird::UIListView>();
    list->setItemProvider(10000, [](size_t i) {
        return "Asset " + std::to_string(i + 1);
    });
    list->setSelectionMode(bird::UISelectionMode::Multiple);  // Ctrl/Shift+click
    list->setWidth(bird::UISizeRule::fill());
    list->setOnActivate([](size_t index) {  // Enter or double-click
        std::cout << "Open asset " << index << std::endl;
    });

    // The tree owns its nodes; only expanded branches produce rows.
    auto* tree = views->add<bird::UITreeView>(std::vector<bird::UITreeNode>{
        bird::UITreeNode::branch("Village",
                                 {bird::UITreeNode::leaf("Main Camera"),
                                  bird::UITreeNode::branch("Houses",
                                                           {bird::UITreeNode::leaf("Inn"),
                                                            bird::UITreeNode::leaf("Smithy")})},
                                 /*expanded=*/true)});
    tree->setWidth(bird::UISizeRule::fill());
    tree->setOnSelectionChanged([](bird::UITreeNode& node, const bird::UITreePath&) {
        std::cout << "Selected " << node.text << std::endl;
    });
    tree->selectPath({0, 1, 0});  // "Inn": expands "Houses" to reveal it

    // Collapsed content takes no space until the header is clicked.
    auto* options = column->add<bird::UIExpander>("Import options", /*expanded=*/false);
    options->getContent()->add<bird::UICheckBox>("Generate mipmaps", true);
}
```

After editing a tree's nodes through `tree->getRoots()`, call `tree->refresh()`. The selection is kept by path where it still exists.

---

## 6. Colour Picker and Rich Inputs

```cpp
#include <scene/2D/UIColorPicker.h>
#include <scene/2D/UIRichInputs.h>

void buildAppearancePage(bird::UIObject* page, std::shared_ptr<bird::Texture> preview) {
    auto* column = page->add<bird::UIStack>(bird::UIAxis::Vertical);
    column->setAnchors(bird::UIAlignment::TopLeft, bird::UIAlignment::BottomRight);
    column->setAlign(bird::UIAlign::Stretch);

    // Full picker: wheel, value/alpha bars, RGB sliders, H/S/V/R/G/B/A
    // fields, and a hex field with Copy and Paste buttons.
    auto* picker = column->add<bird::UIColorPicker>(bird::Vector4(0.0f, 0.5f, 1.0f, 1.0f));
    picker->setOnChanged([](const bird::Vector4& rgba) {
        std::cout << "Accent " << bird::color::toHex(rgba) << std::endl;
    });
    picker->setHex("#FF8000");  // also accepts #RGB, #RGBA and #RRGGBBAA

    // Compact alternative: a swatch button that opens the picker in a popup.
    column->add<bird::UIColorButton>(bird::Vector4(0.9f, 0.3f, 0.2f, 1.0f))
        ->setOnColorChanged([](const bird::Vector4& rgba) { /* apply */ });

    auto* price = column->add<bird::UIRangeSlider>(0.0f, 500.0f, 100.0f, 400.0f);
    price->setStep(10.0f).setMinGap(20.0f);
    price->setOnChanged([](float low, float high) { /* filter */ });

    // Stores the raw GLFW key code; 32 is GLFW_KEY_SPACE.
    column->add<bird::UIKeyBindField>(32)->setOnChanged([](int keyCode) {
        /* rebind the "jump" action */
    });

    auto* split = column->add<bird::UISplitter>(bird::UIAxis::Horizontal, 0.5f);
    split->setHeight(bird::UISizeRule::fixed(160.0f));
    auto* image = split->getFirst()->add<bird::UIImage>(preview, bird::UIImageFit::Cover);
    image->setAnchors(bird::UIAlignment::TopLeft, bird::UIAlignment::BottomRight);
    image->setCornerRadius(8.0f);
}
```

Load the preview texture through the resource manager, for example `RESOURCE_MANAGER->loadTexture("assets/textures/waterfall.png")` (baked by `BuildTools`).

---

## Next Steps

Now that you have mastered 3D scenes and 2D UI, proceed to **[Tutorial 4: Shaders & Asset Baking](04_shaders_and_baking.md)** to learn how to author custom shaders and bake assets.
