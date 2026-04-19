#include "lit-material.hpp"
#include "../asset-loader.hpp"
#include "../deserialize-utils.hpp"

namespace our {

    void LitMaterial::setup() const {
        Material::setup();
        
        if(albedo) {
            glActiveTexture(GL_TEXTURE0);
            albedo->bind();
            if(sampler) sampler->bind(0);
            shader->set("albedo", 0);
        }
        
        if(specular) {
            glActiveTexture(GL_TEXTURE1);
            specular->bind();
            if(sampler) sampler->bind(1);
            shader->set("specular", 1);
        }
        
        if(roughness) {
            glActiveTexture(GL_TEXTURE2);
            roughness->bind();
            if(sampler) sampler->bind(2);
            shader->set("roughness", 2);
        }
        
        shader->set("albedo_tint", albedo_tint);
        shader->set("specular_tint", specular_tint);
        shader->set("roughness_multiplier", roughness_multiplier);
        shader->set("isAnimated", isAnimated);
    }

    void LitMaterial::teardown() const {
        if(albedo) albedo->unbind();
        if(specular) specular->unbind();
        if(roughness) roughness->unbind();
        if(sampler) {
            sampler->unbind(0);
            sampler->unbind(1);
            sampler->unbind(2);
        }
    }

    void LitMaterial::deserialize(const nlohmann::json& data) {
        Material::deserialize(data);
        
        if(!data.is_object()) return;

        albedo = AssetLoader<Texture2D>::get(data.value("albedo", ""));
        specular = AssetLoader<Texture2D>::get(data.value("specular", ""));
        roughness = AssetLoader<Texture2D>::get(data.value("roughness", ""));
        sampler = AssetLoader<Sampler>::get(data.value("sampler", ""));
        
        albedo_tint = data.value("albedo_tint", albedo_tint);
        specular_tint = data.value("specular_tint", specular_tint);
        roughness_multiplier = data.value("roughness_multiplier", roughness_multiplier);
        isAnimated = data.value("isAnimated", isAnimated);
    }

}