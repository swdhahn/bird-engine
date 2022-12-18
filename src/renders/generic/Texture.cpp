//
// Created by Seth Hahn on 11/17/22.
//

#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../util/stb_image.h"
#include "../opengl/GLTexture.h"
#include "../vulkan/VKTexture.h"

namespace bird {

    TextureBuilder::TextureBuilder(void* pData, uint32_t width, uint32_t height, TextureFormat format)
    : m_pData(pData), m_width(width), m_height(height), m_format(format) {}

    TextureBuilder& TextureBuilder::generateMipMaps() {
        m_generateMipMaps = true;
        return *this;
    }

    TextureBuilder& TextureBuilder::filterLinear() {
        m_linearFiltering = true;
        return *this;
    }

    TextureBuilder& TextureBuilder::filterNearest() {
        m_nearestFiltering = true;
        return *this;
    }

    std::unique_ptr<Texture> TextureBuilder::build() {
        if(m_generateMipMaps) { // TODO: actually finish this function with mipmap generation and all

        }

        std::unique_ptr<Texture> tex;
        if(GraphicsPipeline::getGraphicsPipelineType() == GRAPHICS_PIPELINE_OPENGL) {
            uint32_t texID;
            glGenTextures(1, &texID);
            glBindTexture(GL_TEXTURE_2D, texID);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, m_pData);
            glGenerateMipmap(GL_TEXTURE_2D);

            tex = std::make_unique<gl::GLTexture>(m_pData, m_width, m_height, m_format, texID);
            glBindTexture(GL_TEXTURE_2D, 0);
        } else {
            throw std::runtime_error("Graphics pipeline not supported for building images!");
        }


        return tex;
    }

    Texture::Texture(void* pData, uint32_t width, uint32_t height, TextureFormat format)
    : m_pData(pData), m_width(width), m_height(height), m_format(format) {}

    Texture::~Texture() {
        delete m_pData;
    }

    TextureBuilder Texture::create(const std::string& path) {
        int32_t width, height, channels;
        uint8_t* pixels = stbi_load("../textures/texture.jpg", &width, &height, &channels, STBI_rgb_alpha);
        // TODO: Temporary solution.
        //  Real solution: Only load binary versions of files, add .bird extension
        //  and check if exists/if not create it
        uint8_t* nPixels = new uint8_t[width*height*channels];
        memcpy(nPixels, pixels, width*height*channels);
        stbi_image_free(pixels);
        return create(nPixels, width, height, FORMAT_8RGBA);
    }

    TextureBuilder Texture::create(void* data, uint32_t width, uint32_t height, TextureFormat type) {
        return TextureBuilder(data, width, height, type);
    }

} // bird