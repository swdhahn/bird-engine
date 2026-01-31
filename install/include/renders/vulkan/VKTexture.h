//
// Created by Seth Hahn on 11/8/22.
//

#ifndef BIRD_VKTEXTURE_H
#define BIRD_VKTEXTURE_H

#include "VKConstants.h"
#include "../generic/Texture.h"

namespace bird::vk {

    enum TextureTypes {

    };

    class TextureBuilder {
    public:

        TextureBuilder& data(const std::string& path);
        TextureBuilder& textureType(TextureTypes types);

        //Texture
    private:
        uint32_t m_width, m_height;

    };

    class VKTexture : public Texture {
    public:
        VKTexture();


        static TextureBuilder builder();
    private:
        VkImageView m_imageView;
        VkSampler m_sampler;

    };

} // bird

#endif //BIRD_VKTEXTURE_H
