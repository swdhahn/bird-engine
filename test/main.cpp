#include <Application.h>
#include <scene/2D/UIButton.h>
#include <scene/2D/UICheckBox.h>
#include <scene/2D/UIDialog.h>
#include <scene/2D/UILabel.h>
#include <scene/2D/UIPanel.h>
#include <scene/2D/UIProgressBar.h>
#include <scene/2D/UIScrollView.h>
#include <scene/2D/UISlider.h>
#include <scene/2D/UIStack.h>
#include <scene/2D/UITabView.h>
#include <scene/2D/UITextBox.h>
#include <scene/2D/UIWindow.h>
#include <scene/2D/UIManager.h>
#include <scene/Camera.h>
#include <scene/components/MeshComponent.h>
#include <scene/components/Script.h>
#include <util/Assets.h>
#include <util/Math.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class POVCamera : public bird::ProcessScript<bird::Camera> {
   public:
	POVCamera() {}

	~POVCamera() {}

	void init() {
		object()->getWorldEnvironment().clearColor =
			bird::Vector3(0.2, 0.6, 0.85);
	}

	void deinit() {}

	void process(float delta) {
		float speed = 10;
		float rotSpeed = 5;
		float rotVal = input()->getActionValue("look_right") -
					   input()->getActionValue("look_left");
		object()->rotateLocal(bird::Vector3(0, 1, 0),
							  rotVal * rotSpeed * delta);

		object()->translate(
			bird::Vector3((-input()->getActionValue("move_right") +
						   input()->getActionValue("move_left")) *
							  speed * delta,
						  (-input()->getActionValue("move_up") +
						   input()->getActionValue("move_down")) *
							  speed * delta,
						  (-input()->getActionValue("move_backward") +
						   input()->getActionValue("move_forward")) *
							  speed * delta) *
			object()->getLocalRotation() * object()->getWorldRotation());
	}
};


namespace gallery {

using namespace bird;

/** A labelled row: fixed-width caption, then a control that fills. */
UIStack* row(UIObject* parent, const std::string& caption) {
	auto* line = parent->add<UIStack>(UIAxis::Horizontal);
	line->setAlign(UIAlign::Center);
	line->add<UILabel>(caption)->setWidth(UISizeRule::fixed(70.0f));
	return line;
}

void buildControlsTab(UIManager* ui, UIObject* page) {
	auto* column = page->add<UIStack>(UIAxis::Vertical);
	column->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	column->setAlign(UIAlign::Stretch);
	column->setGap(10.0f);

	auto* status = column->add<UILabel>("Interact with the controls below");
	status->setColor(Vector4(0.55f, 0.75f, 1.0f, 1.0f));

	auto* name = row(column, "Name")->add<UITextBox>();
	name->setPlaceholder("Type a name, press Enter");
	name->setWidth(UISizeRule::fill());
	name->setOnSubmit([status](const std::string& text) {
		status->setText("Submitted: " + text);
	});

	column->add<UICheckBox>("Enable shadows", true)
		->setOnChanged([status](bool on) {
			status->setText(on ? "Shadows on" : "Shadows off");
		});

	auto* progress = column->add<UIProgressBar>(0.35f);
	auto* volume = row(column, "Volume")->add<UISlider>(0.0f, 1.0f, 0.35f);
	volume->setWidth(UISizeRule::fill());
	volume->setStep(0.05f);
	volume->setOnChanged([progress](float value) { progress->setValue(value); });
	volume->setTooltip("Drag, click, or use arrow keys");

	auto* buttons = column->add<UIStack>(UIAxis::Horizontal);
	buttons->add<UIButton>("Save")
		->setOnClick([status] { status->setText("Saved (not really)"); })
		.setTooltip("Writes settings to disk");
	buttons->add<UIButton>("Open dialog")->setOnClick([ui, status] {
		auto dialog = std::make_unique<UIDialog>(
			"Discard changes?", "Unsaved settings will be lost.");
		dialog->addButton("Cancel", [status] { status->setText("Kept changes"); });
		dialog->addButton("Discard", [status] { status->setText("Discarded"); });
		ui->pushModal(std::move(dialog));
	});
	buttons->add<UIButton>("Disabled")->setEnabled(false);
}

void buildLayoutTab(UIObject* page) {
	auto* column = page->add<UIStack>(UIAxis::Vertical);
	column->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	column->setAlign(UIAlign::Stretch);
	column->setGap(8.0f);

	const std::pair<const char*, UIJustify> modes[] = {
		{"Start", UIJustify::Start},
		{"Center", UIJustify::Center},
		{"End", UIJustify::End},
		{"SpaceBetween", UIJustify::SpaceBetween}};
	for (const auto& [name, justify] : modes) {
		column->add<UILabel>(std::string("justify ") + name)
			->setColor(Vector4(0.6f, 0.62f, 0.66f, 1.0f));
		auto* line = column->add<UIStack>(UIAxis::Horizontal);
		line->setJustify(justify);
		for (int i = 0; i < 3; i++) {
			auto* box = line->add<UIPanel>();
			box->setSize(Vector2(48.0f, 20.0f));
			box->setBackground(Vector4(0.0f, 0.48f, 0.8f, 0.35f + 0.2f * i));
		}
	}
	column->add<UILabel>("fill weights 1 : 2 : 1")
		->setColor(Vector4(0.6f, 0.62f, 0.66f, 1.0f));
	auto* weights = column->add<UIStack>(UIAxis::Horizontal);
	for (float weight : {1.0f, 2.0f, 1.0f}) {
		auto* box = weights->add<UIPanel>();
		box->setWidth(UISizeRule::fill(weight));
		box->setHeight(UISizeRule::fixed(20.0f));
		box->setBackground(Vector4(0.2f, 0.7f, 0.45f, 0.6f));
	}
}

void buildScrollTab(UIObject* page) {
	auto* scroll = page->add<UIScrollView>();
	scroll->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	auto* list = scroll->setContent<UIStack>(UIAxis::Vertical);
	list->setPadding(UIInsets::all(6.0f));
	list->setAlign(UIAlign::Stretch);
	for (int i = 1; i <= 40; i++) {
		auto* line = list->add<UIStack>(UIAxis::Horizontal);
		line->setAlign(UIAlign::Center);
		line->add<UILabel>("Row " + std::to_string(i))->setWidth(UISizeRule::fill());
		line->add<UICheckBox>("", i % 3 == 0);
	}
}

void openGalleryWindow(UIManager* ui) {
	auto* window = ui->getRoot()->add<UIWindow>("UI Gallery", Vector2(440, 420));
	window->setOffset(Vector2(40.0f, 130.0f));
	auto* tabs = window->getContent()->add<UITabView>();
	tabs->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	buildControlsTab(ui, tabs->addTab<UIObject>("Controls"));
	buildLayoutTab(tabs->addTab<UIObject>("Layout"));
	buildScrollTab(tabs->addTab<UIObject>("Scroll"));
}

}  // namespace gallery

void buildGallery(bird::UIManager* ui) {
	using namespace bird;
	auto* title = ui->getRoot()->add<UILabel>("Bird Engine");
	title->setFontSize(56.0f);
	title->setOffset(Vector2(40.0f, 40.0f));

	// Anchored to the bottom-right corner; follows window resizes.
	auto* reopen = ui->getRoot()->add<UIButton>("Open UI Gallery");
	reopen->setAnchor(UIAlignment::BottomRight);
	reopen->setPivot(UIAlignment::BottomRight);
	reopen->setOffset(Vector2(-16.0f, -16.0f));
	reopen->setOnClick([ui] { gallery::openGalleryWindow(ui); });

	gallery::openGalleryWindow(ui);
}

class MainScript : public bird::Script<bird::Scene> {
   public:
	void init() override {
		bird::INPUT->addAction(bird::Action("move_forward").addKey(KEY_W));
		bird::INPUT->addAction(bird::Action("move_backward").addKey(KEY_S));
		bird::INPUT->addAction(bird::Action("move_left").addKey(KEY_A));
		bird::INPUT->addAction(bird::Action("move_right").addKey(KEY_D));
		bird::INPUT->addAction(bird::Action("move_up").addKey(KEY_SPACE));
		bird::INPUT->addAction(
			bird::Action("move_down").addKey(KEY_LEFT_SHIFT));
		bird::INPUT->addAction(bird::Action("look_up").addKey(KEY_UP));
		bird::INPUT->addAction(bird::Action("look_down").addKey(KEY_DOWN));
		bird::INPUT->addAction(bird::Action("look_left").addKey(KEY_LEFT));
		bird::INPUT->addAction(bird::Action("look_right").addKey(KEY_RIGHT));

		auto e = bird::_own_mk<bird::WorldObject>();
		e->addComponent(bird::_own_mk<bird::MeshComponent>(
			resource_manager()->loadMesh("assets/models/maps/Map1.glb")));
		e->translate(bird::Vector3(0, -5, 30));
		object()->addChild(std::move(e));

		auto e2 = bird::_own_mk<bird::WorldObject>();
		e2->addComponent(bird::_own_mk<bird::MeshComponent>(
			resource_manager()->loadMesh("assets/models/cube.obj")));
		e2->translate(bird::Vector3(0, 5, 0));
		object()->addChild(std::move(e2));

		auto cam = bird::_own_mk<bird::Camera>(1280, 720);
		cam->addComponent(bird::_own_mk<POVCamera>());
		cam->enable();
		object()->addChild(std::move(cam));
		buildGallery(bird::APPLICATION->getUIManager());
	}

	void deinit() override {}

   private:
};

extern "C" BIRD_API_EXPORT bird::Script<bird::Scene>* app_entry() {
	return new MainScript;
}
