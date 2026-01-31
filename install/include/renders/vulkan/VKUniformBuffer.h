//
// Created by Seth Hahn on 11/8/22.
//

#ifndef BIRD_VKUNIFORMBUFFER_H
#define BIRD_VKUNIFORMBUFFER_H

#include "VKConstants.h"
#include "VKTexture.h"

namespace bird::vk {

    class VKUniformBuffer {
    public:
        VKUniformBuffer();

        void createUniformBuffer(void* pData, size_t size);
        void createUniformBuffer(Texture* texture);

    private:
        void* m_pData;
        size_t m_size;
        VKTexture* m_pTexture = nullptr;

    };

} // bird

#endif //BIRD_VKUNIFORMBUFFER_H
