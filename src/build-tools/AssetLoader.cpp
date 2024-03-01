//
// Created by Seth Hahn on 2/23/24.
//

#include "AssetLoader.h"

namespace bird {

    enum AssetType {
        TEXTURE,
        MODEL,
        SCENE,
        SOUND,
        SHADER,
        MATERIAL,
        CUSTOM
    };

    std::vector<std::vector<const char*>> supportedFileTypes {
            {".png", ".jpg", ".jpeg"}, // Textures
            {".obj", ".fbx", ".gltf"}, // 3D models
            {".gltf"}, // Scenes
            {".wav", ".mp3", ".ogg"}, // Sounds
            {".glsl", ".vert", ".frag", ".hlsl", ".metal", ".spv"}, // Shaders
            {".mtl", ".pbr"}, // Materials
            {".bird_custom"} // Other
    };//*/

    AssetLoader::AssetLoader() {}
    AssetLoader::~AssetLoader() {}

    void AssetLoader::init(std::string assetPath) {
        // load assets from file

        for(const auto& entry : std::filesystem::recursive_directory_iterator(assetPath)) {
            if(entry.is_directory()) {
                continue;
            }
            int i = 0;
            bool fileSupported = false;
            std::string filePath = entry.path().c_str();
            for(;i < supportedFileTypes.size(); i++) {
                for(const auto& ext : supportedFileTypes[i]) {
                    if(entry.path().extension() == ext) {
                        fileSupported = true;
                        break;
                    }
                }
                if(fileSupported) {
                    break;
                }
            }
            if(!fileSupported) {
                i = -1;
            }

            // Check if generated asset exists / needs updating
            switch ((AssetType)i) {
                case TEXTURE:
                    loadTexture(filePath);
                    break;
                case MODEL:
                    loadModel(filePath);
                    break;
                case SCENE:
                    loadScene(filePath);
                    break;
                case SOUND:
                    loadSound(filePath);
                    break;
                case SHADER:
                    loadShader(filePath);
                    break;
                case MATERIAL:
                    loadMaterial(filePath);
                    break;
                case CUSTOM:
                    loadCustom(filePath);
                    break;
                default:
                    // Do nothing if file is not supported. This is not an error. .txt, etc.
                    break;
            }
        }



    }

    void AssetLoader::loadAsset() {
        // load asset







    }

    void AssetLoader::loadScene(std::string& path) {
        // load scene
    }

    void AssetLoader::loadModel(std::string& path) {
        // load model
        if(std::filesystem::exists(path + ".bird")) {
            return;
        }

        std::cout << "Compiling Model: " << path << std::endl;
        const aiScene* scene = importer.ReadFile(path,
                                                 aiProcess_CalcTangentSpace | aiProcess_Triangulate |
                                                 aiProcess_SortByPType | aiProcess_JoinIdenticalVertices |
                                                 aiProcess_FixInfacingNormals);
        if(scene == nullptr) {
            std::cout << "Error loading scene" << std::endl;
            std::cout << importer.GetErrorString() << std::endl;
            return;
        }

        std::cout << "Model Compiled. Saving file" << std::endl;
        std::fstream file(path + ".bird", std::ios::out | std::ios::binary);

        // file will be in the following format:
        /*
         * Number of meshes
         * Material index // Mesh 0 start
         * Number of extra buffer data points // vertices, indices, normals, texCoords, etc.  This will be 0 if it's just the standard vertices to texCoords
         * Number of indices
         * Index 0
         * Index 1
         * ...
         * Number of vertices
         * Number of floats per vertex
         * Vertex 0
         * Vertex 1
         * ...
         * Number of normals
         * number of floats per normal
         * normal 0
         * normal 1
         * ...
         * Number of texCoords
         * Number of floats per texCoord
         * texCoord 0
         * texCoord 1
         * ...
         * Number of indices // Mesh 1 start
         * ...
         */
        int zero = 0;
        int two = 2;
        int three = 3;
        file.write((char*)&scene->mNumMeshes, sizeof(uint16_t));

        for(int i = 0; i < scene->mNumMeshes; i++) {
            file.write((char*)&scene->mMeshes[i]->mMaterialIndex, sizeof(uint16_t)); // material index
            file.write((char*)&zero, sizeof(uint32_t)); // extra buffer data points

            int numIndices = 0;
            for(int j = 0; j < scene->mMeshes[i]->mNumFaces; j++) {
                numIndices += scene->mMeshes[i]->mFaces[j].mNumIndices;
            }
            file.write((char*)&numIndices, sizeof(uint32_t)); // number of indices
            for(int j = 0; j < scene->mMeshes[i]->mNumFaces; j++) {
                file.write((char*)&scene->mMeshes[i]->mFaces[j].mIndices[0], sizeof(uint32_t) * scene->mMeshes[i]->mFaces[j].mNumIndices);
            }

            file.write((char*)&scene->mMeshes[i]->mNumVertices, sizeof(uint32_t));
            file.write((char*)&three, sizeof(uint8_t)); // dimension per vertex
            file.write((char*)&scene->mMeshes[i]->mVertices[0], scene->mMeshes[i]->mNumVertices * three * sizeof(float));

            if(scene->mMeshes[i]->HasNormals()) {
                file.write((char*)&scene->mMeshes[i]->mNumVertices, sizeof(uint32_t));
                file.write((char*)&three, sizeof(uint8_t)); // dimension per normal
                file.write((char*)&scene->mMeshes[i]->mNormals[0], scene->mMeshes[i]->mNumVertices * three * sizeof(float));
            } else {
                file.write((char*)&zero, sizeof(uint32_t));
                file.write((char*)&three, sizeof(uint8_t));
            }
            if(!scene->mMeshes[i]->HasTextureCoords(0)) {
                file.write((char*)&zero, sizeof(uint32_t));
                file.write((char*)&two, sizeof(uint8_t));
            } else {
                file.write((char*)&scene->mMeshes[i]->mNumVertices, sizeof(uint32_t));
                file.write((char*)&two, sizeof(uint8_t));
                file.write((char*)&scene->mMeshes[i]->mTextureCoords[0], scene->mMeshes[i]->mNumVertices * two * sizeof(float));
            }

            // handle extra buffer data, it is currently 0 but we'll allow custom data to be added later

        }

        file.close();
        std::cout << "File saved" << std::endl;


    }

    void AssetLoader::loadTexture(std::string& path) {
        // load texture
    }

    void AssetLoader::loadSound(std::string& path) {
        // load sound
    }

    void AssetLoader::loadShader(std::string& path) {
        // load shader
    }

    void AssetLoader::loadMaterial(std::string& path) {
        // load material
        // load model
        if(std::filesystem::exists(path + ".bird")) {
            return;
        }

        std::cout << "Compiling Material: " << path << std::endl;
        const aiScene* scene = nullptr;//importer.

        if(scene == nullptr) {
            std::cout << "Error loading scene" << std::endl;
            std::cout << importer.GetErrorString() << std::endl;
            return;
        }

        std::cout << "Material Compiled. Saving file" << std::endl;
        std::fstream file(path + ".bird", std::ios::out | std::ios::binary);

    }

    void AssetLoader::loadCustom(std::string& path) {
        // load custom
    }
} // bird