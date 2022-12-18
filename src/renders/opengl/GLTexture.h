//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_GLTEXTURE_H
#define BIRD_GLTEXTURE_H

#include "../generic/Texture.h"

namespace bird::gl {

    class GLTexture : public Texture {
    public:
        GLTexture(void* pData, uint32_t width, uint32_t height, TextureFormat format, uint32_t textureID);
        ~GLTexture() override;

        void bind();
        void unbind();
    private:
        uint32_t m_textureID;
    };

} // bird

#endif //BIRD_GLTEXTURE_H
