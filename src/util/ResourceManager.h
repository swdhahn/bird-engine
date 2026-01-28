//
// Created by student on 3/8/24.
//

#ifndef BIRD_RESOURCEMANAGER_H
#define BIRD_RESOURCEMANAGER_H

#include "Resource.h"

#include "../renders/generic/Buffer.h"
#include "../renders/GraphicsConstants.h"

#include "../renders/generic/Texture.h"
#include "../renders/generic/Shader.h"
#include "../renders/generic/Mesh.h"

#include <fstream>
#include <unordered_map>
#include <string>

/*
 * This Resource class includes textures, models, sounds,
 * shaders, and materials.
 *
 * The Resource class is responsible for loading and
 * unloading assets. Also storing assets in an optimized
 * way for both file storage and memory storage.
 */

namespace bird {

    enum AssetType {
        TEXTURE,
        SCENE,
        SOUND,
        SHADER,
        MATERIAL,
        CUSTOM,
        // The above are included in the m_resources variable in the ResourceManager class.
        MESH
    };

    class ResourceManager {
    public:
        ResourceManager();
        ~ResourceManager();

        void init();

        std::shared_ptr<Texture> loadTexture(std::string path);
        std::vector<std::shared_ptr<Mesh>> loadMesh(std::string path);
        std::shared_ptr<Shader> loadShader(ShaderBuilder& builder);
        std::shared_ptr<Material> loadMaterial(std::string path);
        void loadSound(std::string path);
        void loadCustom(std::string path);

        /**
         * Add a resource to the resource manager.
         *
         * @param path The path to the resource. Acts as a name.
         * @param resource The resource to add.
         * @param type The type of resource.
         */
        void addResource(std::string path, std::shared_ptr<Resource> resource, AssetType type);
        void addMesh(std::string path, std::vector<std::shared_ptr<Mesh>> meshes);

    private:
        std::unordered_map<std::string, std::shared_ptr<Resource>>* m_resources;
        std::unordered_map<std::string, std::vector<std::shared_ptr<Mesh>>> m_meshes;

    };









    template <typename T, uint8_t t_attributeSize = 3>
    std::unique_ptr<Buffer<T, t_attributeSize>> createBuffer(BufferMode mode, size_t size) {
        std::unique_ptr<Buffer<T, t_attributeSize>> ptr = nullptr;

        switch(CURRENT_GRAPHICS_PIPELINE) {
            case GRAPHICS_PIPELINE_OPENGL:
                ptr = std::make_unique<bird::gl::GLBuffer<T, t_attributeSize>>(mode, size);

                break;
            default:
                throw std::runtime_error("No other graphics pipelines currently support shaders...");
        }
        return ptr;
    }

    template <typename T, uint8_t t_attributeSize = 3>
    std::unique_ptr<Buffer<T, t_attributeSize>> createBuffer(BufferMode mode, std::unique_ptr<T[]> pData, size_t size) {
        std::unique_ptr<Buffer<T, t_attributeSize>> ptr = nullptr;

        switch(CURRENT_GRAPHICS_PIPELINE) {
            case GRAPHICS_PIPELINE_OPENGL:
                ptr = std::make_unique<bird::gl::GLBuffer<T, t_attributeSize>>(mode, std::move(pData), size);

                break;
            default:
                throw std::runtime_error("No other graphics pipelines currently support shaders...");
        }
        return std::move(ptr);
    }

} // bird

#endif //BIRD_RESOURCEMANAGER_H
