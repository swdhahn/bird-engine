//
// Created by Seth Hahn on 11/8/22.
//

#ifndef BIRD_VKTEXTURE_H
#define BIRD_VKTEXTURE_H

#include <string>
#include <volk.h>

#include "VKConstants.h"
#include "../generic/Texture.h"

namespace bird::vk {

    enum TextureTypes {

    };

    /**
     * Fluent builder for Vulkan textures.
     */
    class TextureBuilder {
    public:
        TextureBuilder& data(const std::string& path);
        TextureBuilder& textureType(TextureTypes types);

    private:
        uint32_t m_width, m_height;
    };

    /**
     * Vulkan 2D texture implementation wrapping VkImage, VkImageView, and VkSampler.
     */
    class VKTexture : public Texture {
    public:
        VKTexture();

        static TextureBuilder builder();
    private:
        VkImageView m_imageView;
        VkSampler m_sampler;
    };

} // namespace bird::vk

#endif //BIRD_VKTEXTURE_H
