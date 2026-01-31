#include <Application.h>
#include <scene/Camera.h>
#include <scene/components/MeshComponent.h>
#include <scene/components/PhysicsBodyComponent.h>
#include <util/Assets.h>
#include <util/Math.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

class TestEntity : public bird::Entity {
   public:
	TestEntity() : bird::Entity() {}

	~TestEntity() {}

	void init() {}

	void deinit() {}

	void process(float delta) {}
};

class POVCamera : public bird::Camera {
   public:
	POVCamera() : bird::Camera(1280, 720) {}
	~POVCamera() {}

	void init() {
		bird::INPUT->addAction(bird::Action("spawn").addKey(KEY_SEMICOLON));
		bird::INPUT->addAction(bird::Action("look_down").addKey(KEY_DOWN));
		bird::INPUT->addAction(bird::Action("look_up").addKey(KEY_UP));
	}

	void deinit() {}

	void process(float delta) {
		float speed = 10;
		float rotSpeed = 5;
		float rotVal = -bird::INPUT->getActionValue("look_right") +
					   bird::INPUT->getActionValue("look_left");
		rotateLocal(bird::Vector3(0, 1, 0), rotVal * rotSpeed * delta);
		rotVal = -bird::INPUT->getActionValue("look_down") + bird::INPUT->getActionValue("look_up");
		rotateLocal(bird::Vector3(1, 0, 0), rotVal * rotSpeed * delta);

		translate(m_localRotation * m_worldRotation *
				  bird::Vector3((bird::INPUT->getActionValue("move_right") -
										bird::INPUT->getActionValue("move_left")) *
										speed * delta,
						  (bird::INPUT->getActionValue("move_up") -
								  bird::INPUT->getActionValue("move_down")) *
								  speed * delta,
						  (bird::INPUT->getActionValue("move_backward") -
								  bird::INPUT->getActionValue("move_forward")) *
								  speed * delta));
	}
};

class MainScene : public bird::Scene {
   public:
	void init() override {
		std::shared_ptr<bird::Shader> shader =
				bird::RESOURCE_MANAGER->loadShader(bird::ShaderBuilder()
								.attachShaderFile(
										"assets/shaders/default.vert", bird::ShaderPipeline::VERTEX)
								.attachShaderFile("assets/shaders/default.frag",
										bird::ShaderPipeline::FRAGMENT));

		auto shader2 = bird::RESOURCE_MANAGER->loadShader(bird::ShaderBuilder()
						.attachShaderFile("assets/shaders/basic.vert", bird::ShaderPipeline::VERTEX)
						.attachShaderFile(
								"assets/shaders/basic.frag", bird::ShaderPipeline::FRAGMENT));
		auto mesh = bird::RESOURCE_MANAGER->loadMesh("assets/models/bugatti/bugatti.obj");
		auto mat = std::make_shared<bird::Material>();
		mat->setShader(shader);
		for (int i = 0; i < mesh.size(); i++) {
			if (mesh[i]->getMaterial() == nullptr) {
				mesh[i]->setMaterial(mat);
			} else {
				mesh[i]->getMaterial()->setShader(shader);
			}
		}
		auto mesh2 = bird::RESOURCE_MANAGER->loadMesh("assets/models/cube.obj");
		auto mat2 = std::make_shared<bird::Material>();
		mat2->setShader(shader2);

		for (int i = 0; i < mesh2.size(); i++) {
			if (mesh2[i]->getMaterial() == nullptr) {
				mesh2[i]->setMaterial(mat2);
			} else {
				mesh2[i]->getMaterial()->setShader(shader2);
			}
		}

		/*std::unique_ptr<TestEntity> e = std::make_unique<TestEntity>();
		e->addComponent(std::make_unique<bird::MeshComponent>(mesh));
		e->translate(bird::Vector3(0, 0, 0));
		this->addEntity(std::move(e));//*/
		for (int i = 0; i < 10; i++) {
			for (int j = 0; j < 10; j++) {
				std::unique_ptr<TestEntity> e2 = std::make_unique<TestEntity>();
				e2->translate(bird::Vector3(i * 0.1, i * 20 + j, j * 0.1));
				e2->addComponent(std::make_unique<bird::MeshComponent>(mesh2));
				e2->addComponent(std::make_unique<bird::PhysicsBodyComponent>());
				this->addEntity(std::move(e2));
			}
		}

		cam = new POVCamera();
		cam->translate(bird::Vector3(0, 0, 40));
		cam->setScene(this);
		cam->init();
		bird::APPLICATION->setCamera(cam);
	}

	void deinit() override {}

   private:
	POVCamera* cam = nullptr;
};

extern "C" BIRD_API_EXPORT bird::Scene* app_entry() {
	return new MainScene;
}
