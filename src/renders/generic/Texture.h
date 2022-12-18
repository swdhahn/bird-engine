//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_TEXTURE_H
#define BIRD_TEXTURE_H

#include <cstdint>
#include <string>
#include "../../util/Constants.h"
#include "../../util/stb_image.h"
#include "../GraphicsPipeline.h"

namespace bird {

    enum TextureFormat {
        FORMAT_HAS_MIPMAPS = 0x1,
        FORMAT_8RGBA = 0x2,
        FORMAT_16RGBA = 0x4
    };

    class Texture;

    class TextureBuilder {
    public:
        TextureBuilder(void* pData, uint32_t width, uint32_t height, TextureFormat format);

        TextureBuilder& generateMipMaps();
        TextureBuilder& filterLinear();
        TextureBuilder& filterNearest();

        std::unique_ptr<Texture> build();
    private:
        void* m_pData = nullptr;
        uint32_t m_width, m_height, m_channels;
        TextureFormat m_format;
        bool m_generateMipMaps = false;
        bool m_linearFiltering = false;
        bool m_nearestFiltering = false;

        void createLinearInterpolated(void* srcData, void* dstData, uint32_t nWidth, uint32_t nHeight);
        void createNearestInterpolated(void* srcData, void* dstData, uint32_t nWidth, uint32_t nHeight);

    };

    /**
     * Todo: Add functions that edit textures?
     *
     * Texture is a class that holds a pointer to the texture data itself and the width and height
     * of an image.
     *
     */
    class Texture {
    public:
        explicit Texture(void* data, uint32_t width, uint32_t height, TextureFormat format);
        virtual ~Texture();

        static TextureBuilder create(const std::string& path);
        static TextureBuilder create(void* data, uint32_t width, uint32_t height, TextureFormat format);
    protected:
        void* m_pData;
        uint32_t m_width, m_height;
        TextureFormat m_format;

    private:
        Texture(const Texture& tex);
        Texture(const Texture&& tex);
    };

} // bird

#endif //BIRD_TEXTURE_H
