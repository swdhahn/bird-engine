//
// Created by student on 2/13/24.
//

#ifndef BIRD_ASSETS_H
#define BIRD_ASSETS_H

#include <fstream>
#include <../renders/generic/Buffer.h>
#include <../renders/GraphicsPipeline.h>
#include "../renders/opengl/GLBuffer.h"

/*
 * Assets are the resources that the game uses.
 * This includes textures, models, sounds, shaders,
 * and materials.
 *
 * The Assets class is responsible for loading and
 * unloading assets. Also storing assets in an optimized
 * way for both file storage and memory storage.
 */

namespace bird {

#define LETTER_OFFSET 26+26+2+1+10

    /**
     * The type of asset that is being loaded.
     */
    enum class AssetType {
        TEXTURE,
        MODEL,
        SOUND,
        SHADER,
        MATERIAL,
        CUSTOM
    };

    class AssetsLoader {
    public:
        AssetsLoader() = default;
        ~AssetsLoader() = default;

        void init();

        /**
         * Write loaded assets to the filesystem.
         */
        void writeAssets();

        void loadAsset();

    private:
        /**
         * Create a map of assets. Where each file
         * has a structure but in binary such as:
         * each asset has a pair of (uint8_t, uint64_t, uint8_t)
         * where the uint8_t is the type of asset and
         * the uint64_t is the id of the asset. The last
         * uint8_t is the hashcode of the asset.
         *
         * We begin reading each pair of (uint8_t, uint64_t),
         * there will be no spaces between the pairs and size
         * will be strictly enforced.
         *
         * Files will be saved as a .bird file where the file
         * name is the id of the asset.
         */
        void createAssetMap();

        /**
         * We load the current map into memory and check against
         * the loaded assets to see if there are any new assets.
         * We have to be careful here in case the user has deleted
         * or changed any assets without the program's knowing. This
         * could completely break the assets.
         */
        void updateAssetMap();
        void loadAssetMap();

    };

    namespace Assets {
        void loadTexture(std::string path);
        std::vector<std::shared_ptr<Mesh>> loadMesh(std::string path);
        void loadSound(std::string path);
        void loadShader(std::string path);
        void loadMaterial(std::string path);
        void loadCustom(std::string path);

    }

    template <typename T, uint8_t t_attributeSize = 3>
    std::unique_ptr<Buffer<T, t_attributeSize>> createBuffer(BufferMode mode, size_t size) {
        std::unique_ptr<Buffer<T, t_attributeSize>> ptr = nullptr;

        switch(GraphicsPipeline::getGraphicsPipelineType()) {
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

        switch(GraphicsPipeline::getGraphicsPipelineType()) {
            case GRAPHICS_PIPELINE_OPENGL:
                ptr = std::make_unique<bird::gl::GLBuffer<T, t_attributeSize>>(mode, std::move(pData), size);

                break;
            default:
                throw std::runtime_error("No other graphics pipelines currently support shaders...");
        }
        return std::move(ptr);
    }

}


#endif //BIRD_ASSETS_H
