#pragma once

#include "material.hpp"
#include "../texture/texture2d.hpp"
#include "../texture/sampler.hpp"
#include <glm/vec3.hpp>

namespace our {

    // This material holds properties required for standard lighting operations
    class LitMaterial : public Material {
    public:
        // Diffuse/Albedo texture and sampler
        Texture2D* albedo;
        Sampler* sampler;
        
        // Specular texture and roughness texture
        Texture2D* specular;
        Texture2D* roughness;
        
        // Instead of alpha threshold from TexturedMaterial, we usually handle opacity via albedo map
        glm::vec3 albedo_tint = glm::vec3(1.0f);
        glm::vec3 specular_tint = glm::vec3(1.0f);
        float roughness_multiplier = 1.0f;
        
        bool isAnimated = false;

        void setup() const override;
        void teardown() const override;
        void deserialize(const nlohmann::json& data) override;
    };

}