#include "Application.h"
#include "ApplicationVK.h"
#include "util/Assets.h"
#include <iostream>
#include <stdexcept>
#include <vector>

/*
 *
 * 1. Shader has a list of materials
 * 2. each material has a list of mesh pointers
 * 3. each model has a list of mesh pointers
 *
 * 1. We loop through shaders. loop through materials in each shader.
 * 2. We loop through models. Connect pair shader material and material for each
 * mesh. Loop through meshes.
 *
 *
 */

class TestEntity : public bird::ModelEntity {
public:
  TestEntity(std::vector<std::shared_ptr<bird::Mesh>> m)
      : bird::ModelEntity(m) {}

  ~TestEntity() {}

  void init() {}

  void deinit() {}

  void process(float delta) {
    float speed = 10;
    float rotSpeed = 10;
    translate(bird::Vector3((-bird::INPUT->getActionValue("move_right") +
                             bird::INPUT->getActionValue("move_left")) *
                                speed * delta,
                            (-bird::INPUT->getActionValue("move_up") +
                             bird::INPUT->getActionValue("move_down")) *
                                speed * delta,
                            (-bird::INPUT->getActionValue("move_backward") +
                             bird::INPUT->getActionValue("move_forward")) *
                                speed * delta));
    float rotVal = -bird::INPUT->getActionValue("look_right") +
                   bird::INPUT->getActionValue("look_left");
    rotate(bird::Vector3(0, 1, 0), rotVal * rotSpeed * delta);
  }
};

class MainScene : public bird::Scene {
public:
  void init() override {
    mesh = bird::Assets::loadMesh("assets/models/Lowpoly_tree_sample.obj");
    // mesh = bird::Assets::loadMesh("assets/models/bugatti/bugatti.obj");
    //  mesh = bird::Assets::loadMesh("assets/models/cube.obj");

    e = new TestEntity(mesh);

    this->addEntity(e);

    e2 = new TestEntity(mesh);
    e2->translate(bird::Vector3(2, 0, -20));
    // this->addEntity(e2);
  }

  void deinit() override {
    delete e;
    delete e2;
  }

private:
  std::vector<std::shared_ptr<bird::Mesh>> mesh;
  bird::ModelEntity *e = nullptr;
  bird::ModelEntity *e2 = nullptr;
};

/* TODO:
 *
 * use glBufferSubData for updates and glBufferData for buffer
 * creation/allocation only
 *
 * Finish linear and bicubic/near interpolation for textures
 * Finish transformations
 *
 *
 */
int main() {
  /*bird::ApplicationVK app;
  app.run();//*/

  MainScene scene;
  bird::Application app(scene);

  try {
    app.run();
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS; //*/
}
