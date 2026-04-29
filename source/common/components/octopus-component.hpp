#pragma once

#include "ecs/component.hpp"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace our {

    enum class OctopusState {
        HIDDEN = 0,           // STEP 1: Underwater, invisible, waiting
        SURFACING = 1,        // STEP 2: Rising + playing spawn anim once
        WAIT_BEFORE_FIGHT = 2,// STEP 3: Hold last frame of spawn anim
        FIGHT_START = 3,      // STEP 4: Play skill01 once (fight-start dance)
        COMBAT_IDLE = 4,      // STEP 5: Looping combat idle, boss_ready = true
        ATTACKING = 5,        // Active combat attack
        MOVING = 6,           // Repositioning
        ENRAGED = 7,          // Enraged animation sequence
        DYING = 8,            // Death sequence (permanent)
        ENRAGED_COMBAT = 9,   // Enraged combat with combo attacks
        DEATH_ANIM = 10,      // Playing death animation once
        REVIVING = 11         // Playing skill03 revival animation once
    };

    enum class OctopusAnimation : int {
        ATTACK = 0,
        FINISH = 1,
        ENRAGE_DANCE = 2,             // Mon_PiratesKing_skill01
        RUN_FORWARD = 3,
        DEATH = 4,
        IDLE = 5,
        SKILL_DANCE_START = 6,
        SKILL04_START = 7,            // Mon_PiratesKing_Skill04_Start
        SKILL_DANCE_HALF_HEALTH = 8,
        SKILL_VICTORY = 9,
        SKILL04_LOOP = 10,            // Mon_PiratesKing_Skill04_Loop
        SKILL04_END = 11,             // Mon_PiratesKing_Skill04_End
        SKILL_SPAWN = 12,
        MOVE_LEFT = 13,
        MOVE_RIGHT = 14,
        COMBAT_IDLE = 15,
        ATTACK02 = 1                 // Mon_PiratesKing_Attack02
    };

    class OctopusComponent : public Component {
    public:
        OctopusState state = OctopusState::HIDDEN;
        float health = 500.0f;
        float currentAnimationTime = 0.0f;
        OctopusAnimation currentAnimIndex = OctopusAnimation::IDLE;

        float stateTimer = 0.0f;
        float speed = 1.0f;

        // Spawn sequence configuration
        float spawnDistance = 40.0f;
        float spawnDelay = 2.0f;
        float surfaceDuration = 1.5f;
        float waitBeforeFightDuration = 5.0f;
        bool bossReady = false;
        bool initialized = false;

        // Animation duration tracking (seconds)
        float animElapsedTime = 0.0f;
        float currentAnimDuration = 0.0f;

        // Attack properties
        float attackDamage = 50.0f;
        float attackCooldown = 2.0f;
        float attackTimer = 0.0f;
        float attackDuration = 30.0f;
        float idleDuration = 60.0f;
        float emergeDelay = 10.0f;
        float attackRange = 40.0f;
        int attackCount = 0;
        int consecutiveMisses = 0;
        bool force_reposition = false;
        bool has_enraged = false;          // Enrage triggers only once per fight
        int attack_combo_index = 0;         // 0,1,2 cycle for enraged combo attacks
        int enrage_phase = 0;               // 0=skill01, 1=skill04_start, 2=skill04_loop, 3=skill04_end
        float chaseSpeed = 8.0f;

        // Position properties
        float surfacedY = -10.0f;    // at water level; clip plane cuts below Y=0
        float submergedY = -12.0f; // fully hidden before spawn
        float minFollowDistance = 30.0f;
        float maxFollowDistance = 100.0f;
        float followDistance = 60.0f;

        // This is the animated model id or name loaded via ModelLoader
        std::string modelName;

        // This stores the computed bone matrices for this frame
        std::vector<glm::mat4> finalBonesMatrices;

        // ---- Tentacle hit detection ----
        // Structs for cached tentacle chain data (built once from model bone info)
        struct TentacleChainBone {
            int boneIndex;              // Index into finalBonesMatrices
            glm::mat4 inverseOffset;    // Precomputed glm::inverse(boneOffsetMatrix)
        };

        struct TentacleChain {
            std::string prefix;                        // e.g. "BN_PiratesKing_Tentacle_R_"
            std::vector<TentacleChainBone> bones;      // Sorted by suffix number (01, 02, ...)
            std::vector<glm::vec3> worldPositions;     // Updated every frame
        };

        float tentacleRadius = 3.0f;         // Collision radius of each tentacle segment
        float playerRadius = 1.5f;            // Collision radius of the player
        float hitCooldownDuration = 0.8f;     // Cooldown between hits from same swing
        float hitCooldownTimer = 0.0f;        // Current cooldown timer
        bool isAttackActive = false;           // True during attack swing phase (norm time 0.3-0.8)
        bool hitRegisteredThisSwing = false;   // Whether a hit was registered during current attack swing
        glm::vec3 lastHitPosition = glm::vec3(0.0f); // World-space position of last tentacle hit

        std::vector<TentacleChain> tentacleChains; // Cached tentacle chain data
        bool tentacleChainsBuilt = false;           // Set to true after first build

        // Flat array of tentacle tip positions (last bone of each chain), updated every frame
        std::vector<glm::vec3> tentacle_tip_positions;

        // ---- Attack02 shockwave effect ----
        bool waveActive = false;
        float waveInnerRadius = 0.0f;
        float waveOuterRadius = 0.0f;
        float waveMaxRadius = 40.0f;
        float waveExpandSpeed = 12.0f;
        float waveDamage = 0.0f;
        bool waveHasDealtDamage = false;
        bool waveFired = false;
        glm::vec3 waveOrigin = glm::vec3(0.0f);
        float waveAlpha = 0.75f;

        // Screen water distortion
        float screenWaterTimer = 0.0f;
        float screenWaterDuration = 3.0f;

        // ---- Death & Revival ----
        bool hasRevived = false;           // true after first death and revival
        float damageMultiplier = 1.0f;     // increases after revival to 1.75f
        bool permanentlyDead = false;      // true after second death confirmed
        float revivalHP = 500.0f;          // HP to restore on revival
        bool deathAnimFinished = false;    // tracks when death anim completes
        bool reviveAnimFinished = false;   // tracks when skill03 completes

        // The ID of this component type is "Octopus"
        static std::string getID() { return "Octopus"; }

        // Reads from json
        void deserialize(const nlohmann::json& data) override;
    };

}
