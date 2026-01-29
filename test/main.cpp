#include <Application.h>
#include <scene/Camera.h>
#include <scene/components/MeshComponent.h>
#include <util/Assets.h>
#include <util/Math.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

class POVCamera : public bird::Camera {
   public:
	POVCamera() : bird::Camera(1280, 720) {}
	~POVCamera() {}

	void init() {}

	void deinit() {}

	void process(float delta) {
		float speed = 10;
		float rotSpeed = 5;
		float rotVal = bird::INPUT->getActionValue("look_right") -
					   bird::INPUT->getActionValue("look_left");
		rotateLocal(bird::Vector3(0, 1, 0), rotVal * rotSpeed * delta);

		translate(bird::Vector3((-bird::INPUT->getActionValue("move_right") +
								 bird::INPUT->getActionValue("move_left")) *
									speed * delta,
								(-bird::INPUT->getActionValue("move_up") +
								 bird::INPUT->getActionValue("move_down")) *
									speed * delta,
								(-bird::INPUT->getActionValue("move_backward") +
								 bird::INPUT->getActionValue("move_forward")) *
									speed * delta) *
				  m_localRotation * m_worldRotation);
	}
};

class TestEntity : public bird::Entity {
   public:
	TestEntity() : bird::Entity() {}

	~TestEntity() {}

	void init() {}

	void deinit() {}

	void process(float delta) {}
};

class MainScene : public bird::Scene {
   public:
	void init() override {
		std::shared_ptr<bird::Shader> shader =
			bird::RESOURCE_MANAGER->loadShader(
				bird::ShaderBuilder()
					.attachShaderFile("assets/shaders/default.vert",
									  bird::ShaderPipeline::VERTEX)
					.attachShaderFile("assets/shaders/default.frag",
									  bird::ShaderPipeline::FRAGMENT));

		auto shader2 = bird::RESOURCE_MANAGER->loadShader(
			bird::ShaderBuilder()
				.attachShaderFile("assets/shaders/basic.vert",
								  bird::ShaderPipeline::VERTEX)
				.attachShaderFile("assets/shaders/basic.frag",
								  bird::ShaderPipeline::FRAGMENT));
		mesh = bird::RESOURCE_MANAGER->loadMesh(
			"assets/models/bugatti/bugatti.obj");
		auto mesh2 = bird::RESOURCE_MANAGER->loadMesh("assets/models/cube.obj");

		auto mat = std::make_shared<bird::Material>();
		auto mat2 = std::make_shared<bird::Material>();
		mat->setShader(shader);
		mat2->setShader(shader2);

		for (int i = 0; i < mesh.size(); i++) {
			if (mesh[i]->getMaterial() == nullptr) {
				mesh[i]->setMaterial(mat);
			} else {
				mesh[i]->getMaterial()->setShader(shader);
			}
		}
		for (int i = 0; i < mesh2.size(); i++) {
			if (mesh2[i]->getMaterial() == nullptr) {
				mesh2[i]->setMaterial(mat2);
			} else {
				mesh2[i]->getMaterial()->setShader(shader2);
			}
		}

		e = new TestEntity();
		this->addEntity(e);
		e->addComponent(std::make_unique<bird::MeshComponent>(mesh));
		e->translate(bird::Vector3(0, 0, 0));

		e2 = new TestEntity();
		e2->addComponent(std::make_unique<bird::MeshComponent>(mesh2));
		e2->translate(bird::Vector3(0, 5, 0));
		this->addEntity(e2);

		cam = new POVCamera();
		bird::APPLICATION->setCamera(cam);
	}

	void deinit() override {
		delete e;
		delete e2;
	}

   private:
	std::vector<std::shared_ptr<bird::Mesh>> mesh;
	bird::Entity* e = nullptr;
	bird::Entity* e2 = nullptr;
	POVCamera* cam = nullptr;
};

extern "C" BIRD_API_EXPORT bird::Scene* app_entry() {
	return new MainScene;
}
