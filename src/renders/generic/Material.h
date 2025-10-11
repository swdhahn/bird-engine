//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_MATERIAL_H
#define BIRD_MATERIAL_H

#include "../../util/Resource.h"
#include "Texture.h"
#include "Shader.h"
#include "../../util/Math.h"

namespace bird {

    class Material : public Resource {
    public:
        Material();
        ~Material() = default;

        void read();
        void write();

        Material& setDiffuseColor(const Vector3& color);
        Material& setSpecularColor(const Vector3& color);
        Material& setAmbientColor(const Vector3& color);
        Material& setShininess(const float& shininess);

        Material& setOpacity(const float& opacity);
        Material& setEmission(const Vector3& emission);
        Material& setReflectivity(const float& reflectivity);

        /**
         * Adds a texture to the material. Textures are bound in the order they are added.
         * @param texture
         * @return
         */
        Material& addTexture(std::shared_ptr<Texture> texture);
        Material& setShader(std::shared_ptr<Shader> shader);

        const Vector3& getDiffuseColor() const;
        const Vector3& getSpecularColor() const;
        const Vector3& getAmbientColor() const;
        const float& getShininess() const;

        const float& getOpacity() const;
        const Vector3& getEmission() const;
        const float& getReflectivity() const;

        const std::vector<std::shared_ptr<Texture>>& getTextures() const;
        const std::shared_ptr<Shader>& getShader() const;

    public:
        Vector3 m_diffuseColor;
        Vector3 m_specularColor;
        Vector3 m_ambientColor;
        float m_shininess;

        float m_opacity;
        Vector3 m_emission;
        float m_reflectivity;

        std::vector<std::shared_ptr<Texture>> m_textures;
        std::shared_ptr<Shader> m_shader;
    };

}

#endif //BIRD_MATERIAL_H
