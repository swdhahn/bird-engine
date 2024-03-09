//
// Created by student on 3/8/24.
//

#include "ResourceManager.h"

namespace bird {

    ResourceManager::ResourceManager() {
        m_resources = new std::unordered_map<std::string, std::shared_ptr<Resource>>[CUSTOM + 1];
    }

    ResourceManager::~ResourceManager() {
        delete m_resources;
    }

    void ResourceManager::init() {

    }

    std::shared_ptr<Texture> ResourceManager::loadTexture(std::string path) {
        if(m_resources[TEXTURE].find(path) != m_resources[TEXTURE].end()) {
            return std::static_pointer_cast<Texture>(m_resources[TEXTURE].at(path));
        }

        std::fstream file("../" + path + ".bird", std::ios::in | std::ios::binary);

        if(!file.is_open()) {
            std::cout << "Error loading file: " << path << std::endl;
            return std::shared_ptr<Texture>(0);
        }

        int32_t width = 0, height = 0, channels = 0;

        file.read(reinterpret_cast<char*>(&width), sizeof(int32_t));
        file.read(reinterpret_cast<char*>(&height), sizeof(int32_t));
        file.read(reinterpret_cast<char*>(&channels), sizeof(int32_t));
        uint8_t* data = new uint8_t[width * height * channels];
        file.read(reinterpret_cast<char*>(data), width * height * channels * sizeof(uint8_t));

        if(channels == 0) {
            std::cout << "Error loading file: " << path << std::endl;
            std::cout << "There should be more than zero channels" << std::endl;
            return std::shared_ptr<Texture>(0);
        }
        TextureFormat form = (TextureFormat) (1 << (channels));
        std::cout << "Img: " << width << " " << height << " " << channels << " " << form << std::endl;

        std::shared_ptr<Texture> tex =  TextureBuilder(data, width, height, form).build();
        addResource(path, tex, TEXTURE);
        return tex;
    }

    std::vector<std::shared_ptr<Mesh>> ResourceManager::loadMesh(std::string path) {
        if(m_meshes.find(path) != m_meshes.end()) {
            return m_meshes.at(path);
        }
        // load mesh from file
        std::fstream file("../" + path + ".bird", std::ios::in | std::ios::binary);

        if(!file.is_open()) {
            std::cout << "Error loading file: " << path << std::endl;
            return std::vector<std::shared_ptr<Mesh>>(0);
        }

        uint16_t meshCount = 0;
        file.read(reinterpret_cast<char*>(&meshCount), sizeof(uint16_t));
        std::vector<std::shared_ptr<Mesh>> meshes(meshCount);
        std::cout << meshCount << std::endl;
        for(int i = 0; i < meshCount; i++) {
            std::unique_ptr<uint32_t[]> indices = nullptr;
            uint32_t indexCount = 0;
            std::unique_ptr<float[]> vertices = nullptr;
            uint32_t vertexCount = 0;
            std::unique_ptr<float[]> normals = nullptr;
            uint32_t normalCount = 0;
            std::unique_ptr<float[]> textureCoords = nullptr;
            uint32_t textureCoordCount = 0;

            uint16_t materialIndex = 0;
            file.read(reinterpret_cast<char*>(&materialIndex), sizeof(uint16_t));
            uint32_t extraBufferPoints = 0;
            file.read(reinterpret_cast<char*>(&extraBufferPoints), sizeof(uint32_t));

            file.read(reinterpret_cast<char*>(&indexCount), sizeof(uint32_t));
            indices = std::make_unique<uint32_t[]>(indexCount);
            file.read(reinterpret_cast<char*>(indices.get()), indexCount * sizeof(uint32_t));

            file.read(reinterpret_cast<char*>(&vertexCount), sizeof(uint32_t));
            uint8_t vertexSize = 0;
            file.read(reinterpret_cast<char*>(&vertexSize), sizeof(uint8_t));
            vertices = std::make_unique<float[]>(vertexCount * vertexSize);
            file.read(reinterpret_cast<char*>(vertices.get()), vertexCount * vertexSize * sizeof(float));

            file.read(reinterpret_cast<char*>(&normalCount), sizeof(uint32_t));
            uint8_t normalSize = 0;
            file.read(reinterpret_cast<char*>(&normalSize), sizeof(uint8_t));
            normals = std::make_unique<float[]>(normalCount * normalSize);
            file.read(reinterpret_cast<char*>(normals.get()), normalCount * normalSize * sizeof(float));

            file.read(reinterpret_cast<char*>(&textureCoordCount), sizeof(uint32_t));
            uint8_t textureCoordSize = 0;
            file.read(reinterpret_cast<char*>(&textureCoordSize), sizeof(uint8_t));
            textureCoords = std::make_unique<float[]>(textureCoordCount * textureCoordSize);
            file.read(reinterpret_cast<char*>(textureCoords.get()), textureCoordCount * textureCoordSize * sizeof(float));

            std::unique_ptr<bird::Buffer<uint32_t>> indexBuffer = createBuffer<uint32_t>(static_cast<bird::BufferMode>(bird::BUFFER_STAGED | bird::BUFFER_ELEMENT_ARRAY), std::move(indices), indexCount);
            std::unique_ptr<bird::Buffer<float>> vertexBuffer = createBuffer<float>(static_cast<bird::BufferMode>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY), std::move(vertices), vertexCount * vertexSize);
            std::unique_ptr<bird::Buffer<float>> normalBuffer = createBuffer<float>(static_cast<bird::BufferMode>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY), std::move(normals), normalCount * vertexSize);
            std::unique_ptr<bird::Buffer<float, 2>> textureCoordBuffer = createBuffer<float, 2>(static_cast<bird::BufferMode>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY), std::move(textureCoords), textureCoordCount * vertexSize);
            bird::MeshBuilder mb = bird::MeshBuilder();
            mb.setIndexBuffer(std::move(indexBuffer))
                    .setVertexBuffer(std::move(vertexBuffer))
                    .setNormalBuffer(std::move(normalBuffer))
                    .setTextureCoordBuffer(std::move(textureCoordBuffer));

            // Load Material
            std::shared_ptr<Material> mat = loadMaterial(path + std::to_string(materialIndex) + ".mat");

            mb.setMaterial(mat);
            meshes.data()[i] = mb.build();
            //addResource(path + std::to_string(i), meshes.data()[i], MESH);
        }

        file.close();

        addMesh(path, meshes);

        return meshes;
    }

    std::shared_ptr<Material> ResourceManager::loadMaterial(std::string path) {
        if(m_resources[MATERIAL].find(path) != m_resources[MATERIAL].end()) {
            return std::static_pointer_cast<Material>(m_resources[MATERIAL].at(path));
        }

        // load material from file
        std::fstream file("../" + path + ".bird", std::ios::in | std::ios::binary);

        if(!file.is_open()) {
            std::cout << "Error loading file: " << path << std::endl;
            return std::shared_ptr<Material>(0);
        }


        std::shared_ptr<Material> mat = std::make_shared<Material>();
        file.read(reinterpret_cast<char*>(&mat->m_diffuseColor), sizeof(float) * 3);
        file.read(reinterpret_cast<char*>(&mat->m_specularColor), sizeof(float) * 3);
        file.read(reinterpret_cast<char*>(&mat->m_ambientColor), sizeof(float) * 3);
        file.read(reinterpret_cast<char*>(&mat->m_shininess), sizeof(float));
        file.read(reinterpret_cast<char*>(&mat->m_opacity), sizeof(float));
        file.read(reinterpret_cast<char*>(&mat->m_emission), sizeof(float) * 3);
        file.read(reinterpret_cast<char*>(&mat->m_reflectivity), sizeof(float));

        std::cout << "Diffuse: " << mat->m_diffuseColor << std::endl;

        uint8_t textureCount = 0;
        file.read(reinterpret_cast<char*>(&textureCount), sizeof(uint8_t));
        for(int i = 0; i < textureCount; i++) {
            std::string texPath;
            uint16_t texPathSize = 0;
            file.read(reinterpret_cast<char*>(&texPathSize), sizeof(uint16_t));
            file.read(reinterpret_cast<char*>(&texPath), texPathSize);
            std::filesystem::path p(path);
            p = p.remove_filename();
            texPath = std::string(p.string() + texPath);

            std::cout << "Path: " << "../" + texPath + ".bird" << std::endl;

            std::shared_ptr<Texture> tex = loadTexture("../" + texPath);
            mat->addTexture(tex);

        }

        file.close();

        addResource(path, mat, MATERIAL);

        return mat;

    }

    std::shared_ptr<Shader> ResourceManager::loadShader(ShaderBuilder& builder) {
        std::shared_ptr<Shader> shader = builder.create();
        addResource(builder.m_shaderFiles.data()[0].first, shader, SHADER);
        return shader;
    }

    void ResourceManager::loadSound(std::string path) {

    }

    void ResourceManager::loadCustom(std::string path) {

    }

    void ResourceManager::addResource(std::string path, std::shared_ptr<Resource> resource, AssetType type) {
        if(type == MESH) {
            //throw RuntimeException("Error: Cannot add mesh to resource manager using addResource function. ");
            std::cout << "Error: Cannot add mesh to resource manager using addResource function. " << std::endl;
            return;
        }
        m_resources[type].insert(std::pair<std::string, std::shared_ptr<Resource>>(path, resource));
    }

    void ResourceManager::addMesh(std::string path, std::vector<std::shared_ptr<Mesh>> meshes) {
        m_meshes.insert(std::pair<std::string, std::vector<std::shared_ptr<Mesh>>>(path, meshes));
    }

} // bird