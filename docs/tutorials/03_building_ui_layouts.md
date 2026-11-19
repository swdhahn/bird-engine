# Tutorial 3: Building UI Layouts & Widgets

Bird Engine features an immediate-to-retained 2D UI framework managed by `bird::UIManager`.

In this tutorial, you will learn how to:
1. Access the `UIManager`.
2. Use flexbox-style `UIStack` layouts.
3. Configure layout anchors, pivots, and sizing rules.
4. Add interactive controls (`UIButton`, `UISlider`, `UICheckBox`, `UITextBox`).
5. Wire callbacks and display modal dialogs (`UIDialog`).

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

## Next Steps

Now that you have mastered 3D scenes and 2D UI, proceed to **[Tutorial 4: Shaders & Asset Baking](04_shaders_and_baking.md)** to learn how to author custom shaders and bake assets.
