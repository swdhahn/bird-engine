
#include <stdexcept>
#include <iostream>
#include <vector>
//#include "Application.h"
//#include "vulkan/VKPipeline.h"
#include "renders/opengl/GLPipeline.h"
#include "renders/opengl/GLBuffer.h"

bird::gl::GLMesh* createMesh() {
    const static float cSVertices[] = {
            0.5f,  0.5f, 0.0f,  // top right
            0.5f, -0.5f, 0.0f,  // bottom right
            -0.5f, -0.5f, 0.0f,  // bottom left
            -0.5f,  0.5f, 0.0f   // top left
    };
    const static unsigned int cSIndices[] = {  // note that we start from 0!
            0, 1, 3,   // first triangle
            1, 2, 3    // second triangle
    };
    std::unique_ptr<uint32_t[]> indices = std::make_unique<uint32_t[]>(6);
    std::unique_ptr<float[]> vertices = std::make_unique<float[]>(12);
    memcpy(indices.get(), cSIndices, 6 * sizeof(uint32_t));
    memcpy(vertices.get(), cSVertices, 12 * sizeof(float));
    std::unique_ptr<bird::gl::GLBuffer<uint32_t>> indexBuffer = std::make_unique<bird::gl::GLBuffer<uint32_t>>(static_cast<bird::BufferMode>(bird::BUFFER_STAGED | bird::BUFFER_ELEMENT_ARRAY), std::move(indices), 6);
    std::unique_ptr<bird::gl::GLBuffer<float>> vertexBuffer = std::make_unique<bird::gl::GLBuffer<float>>(static_cast<bird::BufferMode>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY), std::move(vertices), 12);

    return new bird::gl::GLMesh(std::move(indexBuffer), std::move(vertexBuffer), nullptr, nullptr);
}

class TestEntity : public bird::ModelEntity {
public:
    TestEntity(bird::Mesh* m) : bird::ModelEntity(m, 1) {

    }

    ~TestEntity() {

    }

    void init() {

    }

    void deinit() {

    }

    void process(float delta) {

    }

};

class MainScene : public bird::Scene {
public:

    void init() override {
        mesh = createMesh();

        e = new TestEntity(mesh);


        this->addEntity(e);
    }

    void deinit() override {
        delete e;
        delete mesh;
    }

private:
    bird::Mesh* mesh = nullptr;
    bird::ModelEntity* e = nullptr;

};

/* TODO:
 *
 * use glBufferSubData for updates and glBufferData for buffer creation/allocation only
 *
 * Finish linear and bicubic/near interpolation for textures
 * Finish transformations
 *
 *
 */
int main() {
    bird::GraphicsPipeline* pipeline = new bird::gl::GLPipeline;

    MainScene scene;

    try {
        pipeline->init();

        scene.init();

        while(!pipeline->getWindow()->shouldWindowClose()) {
            pipeline->renderScene(&scene);
            pipeline->getWindow()->pollWindow();
        }

        scene.deinit();

        pipeline->cleanUp();
    } catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    delete pipeline;
    //*/

    /*bird::Application app;

    try {
        app.run();
    } catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }//*/

    return EXIT_SUCCESS;
}
