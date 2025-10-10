//
// Created by student on 2/13/24.
//

#include "Assets.h"
#include "generic/Buffer.h"
#include "generic/Mesh.h"

namespace bird {

namespace Assets {

void loadTexture(std::string path) {}

std::vector<std::shared_ptr<Mesh>> loadMesh(std::string path) {
  // load mesh from file
  std::fstream file(path + ".bird", std::ios::in | std::ios::binary);

  if (!file.is_open()) {
    std::cout << "Error loading file: " << path << std::endl;
    return std::vector<std::shared_ptr<Mesh>>(0);
  }

  uint16_t meshCount = 0;
  file.read(reinterpret_cast<char *>(&meshCount), sizeof(uint16_t));
  std::vector<std::shared_ptr<Mesh>> meshes(meshCount);
  std::cout << meshCount << std::endl;
  for (int i = 0; i < meshCount; i++) {
    std::unique_ptr<uint32_t[]> indices = nullptr;
    uint32_t indexCount = 0;
    std::unique_ptr<float[]> vertices = nullptr;
    uint32_t vertexCount = 0;
    std::unique_ptr<float[]> normals = nullptr;
    uint32_t normalCount = 0;
    std::unique_ptr<float[]> textureCoords = nullptr;
    uint32_t textureCoordCount = 0;

    uint16_t materialIndex = 0;
    file.read(reinterpret_cast<char *>(&materialIndex), sizeof(uint16_t));
    uint32_t extraBufferPoints = 0;
    file.read(reinterpret_cast<char *>(&extraBufferPoints), sizeof(uint32_t));

    file.read(reinterpret_cast<char *>(&indexCount), sizeof(uint32_t));
    indices = std::make_unique<uint32_t[]>(indexCount);
    file.read(reinterpret_cast<char *>(indices.get()),
              indexCount * sizeof(uint32_t));

    file.read(reinterpret_cast<char *>(&vertexCount), sizeof(uint32_t));
    uint8_t vertexSize = 0;
    file.read(reinterpret_cast<char *>(&vertexSize), sizeof(uint8_t));
    vertices = std::make_unique<float[]>(vertexCount * vertexSize);
    file.read(reinterpret_cast<char *>(vertices.get()),
              vertexCount * vertexSize * sizeof(float));

    file.read(reinterpret_cast<char *>(&normalCount), sizeof(uint32_t));
    uint8_t normalSize = 0;
    file.read(reinterpret_cast<char *>(&normalSize), sizeof(uint8_t));
    normals = std::make_unique<float[]>(normalCount * normalSize);
    file.read(reinterpret_cast<char *>(normals.get()),
              normalCount * normalSize * sizeof(float));

    file.read(reinterpret_cast<char *>(&textureCoordCount), sizeof(uint32_t));
    uint8_t textureCoordSize = 0;
    file.read(reinterpret_cast<char *>(&textureCoordSize), sizeof(uint8_t));
    textureCoords =
        std::make_unique<float[]>(textureCoordCount * textureCoordSize);
    file.read(reinterpret_cast<char *>(textureCoords.get()),
              textureCoordCount * textureCoordSize * sizeof(float));

    std::unique_ptr<bird::Buffer<uint32_t>> indexBuffer =
        createBuffer<uint32_t>(
            static_cast<bird::BufferMode>(bird::BUFFER_STAGED |
                                          bird::BUFFER_ELEMENT_ARRAY),
            std::move(indices), indexCount);
    std::unique_ptr<bird::Buffer<float>> vertexBuffer = createBuffer<float>(
        static_cast<bird::BufferMode>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY),
        std::move(vertices), vertexCount * vertexSize);
    std::unique_ptr<bird::Buffer<float>> normalBuffer = createBuffer<float>(
        static_cast<bird::BufferMode>(bird::BUFFER_STAGED | bird::BUFFER_ARRAY),
        std::move(normals), normalCount * vertexSize);
    std::unique_ptr<bird::Buffer<float, 2>> textureCoordBuffer =
        createBuffer<float, 2>(static_cast<bird::BufferMode>(
                                   bird::BUFFER_STAGED | bird::BUFFER_ARRAY),
                               std::move(textureCoords), textureCoordCount);
    bird::MeshBuilder mb = bird::MeshBuilder();
    mb.setIndexBuffer(std::move(indexBuffer))
        .setVertexBuffer(std::move(vertexBuffer))
        .setNormalBuffer(std::move(normalBuffer))
        .setTextureCoordBuffer(std::move(textureCoordBuffer));
    meshes.data()[i] = mb.build();
  }

  file.close();

  return meshes;
}

void loadSound(std::string &path);
void loadShader(std::string &path);
void loadMaterial(std::string &path);
void loadCustom(std::string &path);
} // namespace Assets

void AssetsLoader::init() {
  // load assets from file
}

void AssetsLoader::writeAssets() {
  // write assets to file
}

void AssetsLoader::loadAsset() {
  // load asset
}

void AssetsLoader::loadAssetMap() {
  // load asset
}

void AssetsLoader::createAssetMap() {
  // create asset map
  std::ofstream file("assets.map", std::ios::binary);

  file << "ASSETS";

  file.close();
}

void AssetsLoader::updateAssetMap() {}

} // namespace bird
