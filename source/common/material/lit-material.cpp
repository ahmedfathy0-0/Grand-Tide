#include "lit-material.hpp"
#include "../asset-loader.hpp"
#include "../deserialize-utils.hpp"
#include <iostream>

namespace our
{

    void LitMaterial::setup() const
    {
        Material::setup();

        GLint loc = shader->getUniformLocation("albedo");

        if (albedo)
        {
            glActiveTexture(GL_TEXTURE0);
            albedo->bind();
            if (sampler)
                sampler->bind(0);
            shader->set("albedo", 0);
        } else {
            std::cerr << "[LitMaterial] WARNING: albedo texture is null!" << std::endl;
        }

        if (specular)
        {
            glActiveTexture(GL_TEXTURE1);
            specular->bind();
            if (sampler)
                sampler->bind(1);
            shader->set("specular", 1);
        }

        if (roughness)
        {
            glActiveTexture(GL_TEXTURE2);
            roughness->bind();
            if (sampler)
                sampler->bind(2);
            shader->set("roughness", 2);
        }

        if (normal)
        {
            glActiveTexture(GL_TEXTURE3);
            normal->bind();
            if (sampler)
                sampler->bind(3);
            shader->set("normal_map", 3);
        }
        shader->set("has_normal", normal != nullptr);
        shader->set("has_specular", specular != nullptr);
        shader->set("has_roughness", roughness != nullptr);
        shader->set("has_albedo", albedo != nullptr);

        shader->set("albedo_tint", albedo_tint);
        shader->set("specular_tint", specular_tint);
        shader->set("roughness_multiplier", roughness_multiplier);
        shader->set("isAnimated", isAnimated);
        shader->set("uv_multiplier", uv_multiplier);
    }

    void LitMaterial::teardown() const
    {
        if (albedo)
            albedo->unbind();
        if (specular)
            specular->unbind();
        if (roughness)
            roughness->unbind();
        if (normal)
            normal->unbind();
        if (sampler)
        {
            sampler->unbind(0);
            sampler->unbind(1);
            sampler->unbind(2);
            sampler->unbind(3);
        }
    }

    void LitMaterial::deserialize(const nlohmann::json &data)
    {
        Material::deserialize(data);

        if (!data.is_object())
            return;

        albedo = AssetLoader<Texture2D>::get(data.value("albedo", ""));
        specular = AssetLoader<Texture2D>::get(data.value("specular", ""));
        roughness = AssetLoader<Texture2D>::get(data.value("roughness", ""));
        normal = AssetLoader<Texture2D>::get(data.value("normal", ""));
        sampler = AssetLoader<Sampler>::get(data.value("sampler", ""));

        albedo_tint = data.value("albedo_tint", albedo_tint);
        specular_tint = data.value("specular_tint", specular_tint);
        roughness_multiplier = data.value("roughness_multiplier", roughness_multiplier);
        isAnimated = data.value("isAnimated", isAnimated);

        if (data.contains("uv_multiplier"))
        {
            uv_multiplier = glm::vec2(data["uv_multiplier"][0], data["uv_multiplier"][1]);
        }
    }

}