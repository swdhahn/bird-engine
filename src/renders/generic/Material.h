//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MATERIAL_H
#define BIRD_MATERIAL_H

#include "Texture.h"

namespace bird {

    class Material {
    public:
        Material(uint8_t textureCount);
        ~Material();

        Texture **textures;

    private:
    };

}

#endif //BIRD_MATERIAL_H
