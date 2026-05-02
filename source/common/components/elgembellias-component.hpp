#pragma once

#include "ecs/component.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our {

    enum class ElgembelliasState {
        HIDDEN = 0,           // Underwater, invisible, waiting
        SURFACING = 1,        // Rising to the surface
        DANCING = 2,          // Performing dance animation, facing player
        CHASE = 3,            // Running toward player
        ATTACKING = 4,        // Performing kick on player
        DANCE_2 = 5,          // ChickenDance after 3 successful kicks
        MISS_DANCE = 6,       // Brief dance taunt after a miss
        POST_HIT = 7,         // Brief taunt after landing a hit
        DAMAGE = 8,           // Flinch on hit
        DEATH = 9,            // Death animation
    };

    // Animation indices matching the model's Armature| clips
    enum class ElgembelliasAnimation : int {
        CHICKEN_DANCE = 10,   // Armature|chickenDance
        DAMAGE = 11,          // Armature|damage
        DANCE = 12,           // Armature|dance
        DEATH = 13,           // Armature|death
        FALL_DOWN = 14,       // Armature|fallDown
        KICK = 15,            // Armature|kick
        RUN = 17,             // Armature|run
        SALUTE = 18,          // Armature|salute
        STAND_UP = 19,        // Armature|standUp
    };

    class ElgembelliasComponent : public Component {
    public:
        ElgembelliasState state = ElgembelliasState::HIDDEN;

        float stateTimer = 0.0f;
        float surfacedY = -5.0f;
        float submergedY = -150.0f;
        float surfaceSpeed = 5.0f;

        // Current animation
        ElgembelliasAnimation currentAnimIndex = ElgembelliasAnimation::DANCE;
        float animElapsedTime = 0.0f;
        float currentAnimDuration = 0.0f;

        // Spawn configuration
        float spawnDistance = 120.0f;
        float minDistFromOctopus = 60.0f;
        bool spawned = false;

        // Dance timing
        float danceTimer = 0.0f;
        float danceDuration = 15.0f;

        // Combat
        float attackRange = 15.0f;          // Distance to trigger kick
        float chaseSpeed = 8.0f;            // Run speed toward player
        float kickDuration = 1.5f;          // Duration of kick animation
        float kickDamage = 25.0f;           // Damage dealt by kick
        float kickHitRadius = 5.0f;         // Sphere radius for kick hit detection
        float playerRadius = 1.5f;          // Player collision radius
        int hitCounter = 0;                 // Consecutive successful kicks
        int maxHits = 3;                    // Hits before chickenDance
        bool hitRegisteredThisKick = false;  // Whether current kick hit the player
        bool isAttackActive = false;        // Whether kick is in the active swing phase
        float dance2Duration = 4.0f;        // Duration of chickenDance
        float damageDuration = 0.8f;        // Duration of damage flinch
        float missDanceDuration = 2.0f;     // Brief dance after a miss before chasing
        float postHitDanceDuration = 1.5f;  // Brief dance after landing a hit
        float minDistance = 10.0f;           // Minimum distance from player (won't get closer)
        bool forceReposition = false;       // After miss, must close distance before attacking

        // Health
        float maxHealth = 300.0f;
        float currentHealth = 300.0f;
        float previousHealth = 300.0f;      // For detecting damage
        bool justDamaged = false;
        float damageFlashTimer = 0.0f;

        // Bone matrices computed by AnimationSystem
        std::vector<glm::mat4> finalBonesMatrices;

        // The ID of this component is "Elgembellias"
        static std::string getID() { return "Elgembellias"; }

        // Reads from json
        void deserialize(const nlohmann::json& data) override;
    };

}
