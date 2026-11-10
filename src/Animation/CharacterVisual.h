#pragma once
#include "../Game/Character/PlayerAppearance.h"
#include "../Graphics/Mesh.h"
#include "SkeletalAnimation.h"
#include "WeaponVisual.h"
#include <memory>
#include <array>
#include <functional>

class Character;
class Camera;
class Shader;

struct CharacterVisualState
{
    bool dead = false, equipped = false, aiming = false, fired = false;
    float reloadRemaining = 0, aimPitch = 0;
    bool hasWeapon = true;
    bool magazineInserted = true;
};

class CharacterVisual
{
public:
    CharacterVisual() = default;
    ~CharacterVisual();
    CharacterVisual(const CharacterVisual&) = delete;
    CharacterVisual& operator=(const CharacterVisual&) = delete;
    void Init(PlayerAppearance appearance = PlayerAppearance::Female);
    void Update(float dt, const Character& character, const CharacterVisualState& state = {}, float poseInterval = 0.0f);
    void Render(const Character& character, Shader& shader, Camera& camera, float width, float height,
        const glm::vec3& color = glm::vec3(0.3f, 0.8f, 0.4f), bool firstPerson = false);
    void Reset();
    void SetLootPreview(bool enabled);
    void SetLootCorpseFlipped(bool enabled, const Character& character);
    void SetHideoutBedPose(bool enabled, float surfaceHeight = 0.f,
        const glm::vec3& bedCenter = glm::vec3(0));
    float HideoutBedYaw() const { return hideoutBedYaw; }
    void SetHideoutKneelPose(bool enabled);
    glm::vec3 FirstPersonCameraPosition(const Character& character) const;
    glm::vec3 FitLootPose(const Character& character, const Character& corpse,
        const CharacterVisual& corpseVisual,
        const std::function<bool(const glm::vec3&, float, const std::array<glm::vec3, 4>&)>& placementClear,
        float approachYaw = 180.f, float supportHeight = -1.f);
    void CenterDeathPoseOnSurface(Character& character, const glm::vec3& surfaceCenter) const;
    void AlignDeathPosePelvis(Character& character, const glm::vec3& worldPosition) const;
    void AlignLootPosePelvis(Character& character, const glm::vec3& worldPosition) const;
    void AlignLootPoseHead(Character& character, const glm::vec3& worldPosition) const;
    float FittedLootYaw() const { return lootPoseYaw; }
    bool LootPoseValid() const { return lootPoseValid; }
    float LootPoseHeight() const { return lootPoseHeight; }
    const std::array<glm::vec3, 4>& LootSupports() const { return lootSupports; }

private:
    void UploadPalette();
    void AnimateLootSearch(float dt);
    glm::mat4 CorpseRootMatrix() const;
    struct Assets;
    std::shared_ptr<Assets> assets;
    GLuint paletteBuffer = 0, paletteTexture = 0;
    float poseElapsed = 0;
    float backpackClock = 0, backpackMotion = 0;
    bool lootPoseValid = false;
    bool lootCorpseFlipped = false;
    glm::mat4 flippedDeathRoot{1};
    bool hideoutBedPose = false, hideoutBedPoseInitialized = false;
    float hideoutBedSurfaceHeight = 0.f;
    float hideoutBedYaw = 0.f;
    glm::vec3 hideoutBedCenter{0};
    glm::mat4 hideoutBedRoot{1};
    bool hideoutKneelPose = false, hideoutKneelPoseInitialized = false;
    bool lootPreview = false, lootPreviewInitialized = false;
    float lootPoseHeight = .75f;
    float lootPoseYaw = 180.f;
    float lootClock = 0;
    glm::mat4 lootInverse{1};
    std::array<glm::vec3, 2> lootHandTargets{}, lootElbows{};
    glm::vec3 lootPalmDirection{0, -1, 0};
    glm::vec3 lootSweepDirection{0};
    std::array<glm::vec3, 4> lootSupports{};
    SkeletalAnimation animation;
    WeaponVisual weapon;
    glm::mat4 weaponModel{1}; // Character-local meters, shared by IK and rendering.
    MotionPose deathRoot;
    CharacterVisualState state;
    float deathTime = 0, holdWeight = 0;
    float motionClock = 0, aimWeight = 0, recoilTime = 1;
    float jumpWeight = 0, jumpPhase = 0, landingTime = 1;
    bool wasAirborne = false;
    float proneWeight = 0, crawlClock = 0, crawlWeight = 0;
    std::array<float, 5> stanceWeights{};
    bool dying = false;
    bool firePending = false;
};
