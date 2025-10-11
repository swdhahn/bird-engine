//
// Created by Seth Hahn on 11/30/22.
//

#include "Material.h"

namespace bird {

    Material::Material()
    : m_diffuseColor(1), m_specularColor(1), m_ambientColor(1), m_shininess(1),
    m_opacity(1), m_emission(0), m_reflectivity(0)
    {}

    void Material::read() {

    }

    void Material::write() {

    }

    Material& Material::setDiffuseColor(const Vector3& color) {
        m_diffuseColor = color;
        return *this;
    }

    Material& Material::setSpecularColor(const Vector3& color) {
        m_specularColor = color;
        return *this;
    }

    Material& Material::setAmbientColor(const Vector3& color) {
        m_ambientColor = color;
        return *this;
    }

    Material& Material::setShininess(const float& shininess) {
        m_shininess = shininess;
        return *this;
    }

    Material& Material::setOpacity(const float& opacity) {
        m_opacity = opacity;
        return *this;
    }

    Material& Material::setEmission(const Vector3& emission) {
        m_emission = emission;
        return *this;
    }

    Material& Material::setReflectivity(const float& reflectivity) {
        m_reflectivity = reflectivity;
        return *this;
    }

    Material& Material::addTexture(std::shared_ptr<Texture> texture) {
        m_textures.push_back(texture);
        return *this;
    }

    Material& Material::setShader(std::shared_ptr<Shader> shader) {
        m_shader = shader;
        return *this;
    }

    const std::vector<std::shared_ptr<Texture>>& Material::getTextures() const {
        return m_textures;
    }

    const std::shared_ptr<Shader>& Material::getShader() const {
        return m_shader;
    }

    const Vector3& Material::getDiffuseColor() const {
        return m_diffuseColor;
    }

    const Vector3& Material::getSpecularColor() const {
        return m_specularColor;
    }

    const Vector3& Material::getAmbientColor() const {
        return m_ambientColor;
    }

    const float& Material::getShininess() const {
        return m_shininess;
    }

    const float& Material::getOpacity() const {
        return m_opacity;
    }

    const Vector3& Material::getEmission() const {
        return m_emission;
    }

    const float& Material::getReflectivity() const {
        return m_reflectivity;
    }

}
