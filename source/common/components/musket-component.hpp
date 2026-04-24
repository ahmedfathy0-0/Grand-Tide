#pragma once

#include "ecs/component.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our

{

    enum class MusketState
    {
        IDLE = 0,
        FOLLOWING = 1,
        FIRING = 2,
        WAITING_FOR_ANIM = 3 // New state for waiting until fire animation finishes before going back to idle
    };

    class MusketComponent : public Component
    {
    public:
        // Animation data (same pattern as SharkComponent)
        float currentAnimationTime = 0.0f;
        int currentAnimIndex = 0;                  // 0 = idle, 17 = fire
        std::string modelName;                     // FBX model key in ModelLoader::models
        std::vector<glm::mat4> finalBonesMatrices; // Computed bone matrices per frame

        // Fire behavior
        float fireRate = 1.0f;      // Seconds between shots
        float damage;               // Damage dealt to player per shot
        // Identity
        int boatIndex = 0;   // Which boat (0 or 1)
        int musketIndex = 0; // Which musket on the boat (0 or 1)

        MusketState state = MusketState::IDLE; // Added for tracking state dynamically
        float timer = 0.0f;                    // General purpose timer

        static std::string getID() { return "Musket"; }

        void deserialize(const nlohmann::json &data) override;
    };

}
