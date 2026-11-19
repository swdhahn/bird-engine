#include <Application.h>
#include <scene/2D/UIButton.h>
#include <scene/2D/UILabel.h>
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
		auto button = bird::_own_mk<bird::UIButton>();
		button->setScale(bird::Vector2(200, 100));
		button->setPosition(bird::Vector2(600, 400));
		button->setText("Click me");
		button->setOnClick([] { std::cout << "Button clicked" << std::endl; });

		bird::APPLICATION->getUIManager()->getRoot()->addChild(
			std::move(button));

		auto title = bird::_own_mk<bird::UILabel>("Bird Engine");
		title->setFontSize(72.0f);
		title->setPosition(bird::Vector2(40, 40));
		bird::APPLICATION->getUIManager()->getRoot()->addChild(
			std::move(title));
	}

	void deinit() override {}

   private:
};

extern "C" BIRD_API_EXPORT bird::Script<bird::Scene>* app_entry() {
	return new MainScript;
}
