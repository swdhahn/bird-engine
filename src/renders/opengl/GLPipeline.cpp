//
// Created by Seth Hahn on 11/17/22.
//

#include "GLPipeline.h"

namespace bird::gl {

    GLPipeline::GLPipeline() : GraphicsPipeline(GRAPHICS_PIPELINE_OPENGL) {

    }

    GLPipeline::~GLPipeline() {

    }

    void GLPipeline::init() {
        m_pWindow = std::make_unique<Window>(1280, 720, "Window");

        glewExperimental = true;
        int code = 0;
        if((code = glewInit())) {
            std::cerr << glewGetErrorString(code) << std::endl;
            throw std::runtime_error("Couldn't initialize GLEW!");
        }


        const char *vertexShaderSource = "#version 330 core\n"
                                         "layout (location = 0) in vec3 aPos;\n"
                                         "void main()\n"
                                         "{\n"
                                         "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                         "}\0";
        const char *fragmentShaderSource = "#version 330 core\n"
                                           "out vec4 FragColor;\n"
                                           "void main()\n"
                                           "{\n"
                                           "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
                                           "}\n\0";


        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        // check for shader compile errors
        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        }
        // fragment shader
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        // check for shader compile errors
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        }//*/
        /*int success;
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        size_t length=0;
        std::unique_ptr<std::byte[]> bytes = readBinaryFile("shaders/vertex2.vert.spv", length);
        glShaderBinary(1, &vertexShader, GL_SHADER_BINARY_FORMAT_SPIR_V, bytes.get(), length);
        glSpecializeShader(vertexShader, "main", 0, nullptr, nullptr);
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        bytes = readBinaryFile("shaders/fragment2.vert.spv", length);
        glShaderBinary(1, &vertexShader, GL_SHADER_BINARY_FORMAT_SPIR_V, bytes.get(), length);
        glSpecializeShader(fragmentShader, "main", 0, nullptr, nullptr);//*/
        // link shaders
        unsigned int shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        // check for linking errors
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
        //    glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        //    std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        glUseProgram(shaderProgram);
    }

    void GLPipeline::renderScene(const bird::Scene *scene) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0, 0, 0, 1);

        std::vector<ModelEntity*> entities;
        getModelEntities(scene, entities);

        // cull entities here

        // render entities
        for(int i = 0; i < entities.size(); i++) {
            ModelEntity* e = entities[i];
            for(int j = 0; j < e->getMeshCount(); j++) {
                const GLMesh* m = dynamic_cast<const GLMesh*>(&e->getMeshes()[j]);
                glBindVertexArray(m->getVAO());
                glDrawElements(GL_TRIANGLES, m->getIndexBuffer()->getBufferSize(), GL_UNSIGNED_INT, 0);
            }
        }

        //glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(m_pWindow->getGLFWWindow());
    }

    void GLPipeline::cleanUp() {
        m_pWindow.reset();
    }

    std::string GLPipeline::getName() const {
        return "Opengl Pipeline";
    }

} // bird