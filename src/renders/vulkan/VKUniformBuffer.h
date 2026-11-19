//
// Created by Seth Hahn on 11/8/22.
//

#ifndef BIRD_VKUNIFORMBUFFER_H
#define BIRD_VKUNIFORMBUFFER_H

#include <cstddef>
#include <volk.h>

#include "VKConstants.h"
#include "VKTexture.h"

namespace bird::vk {

    /**
     * Vulkan uniform buffer handle managing host-visible device memory and descriptor updates.
     */
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

} // namespace bird::vk

#endif //BIRD_VKUNIFORMBUFFER_H
