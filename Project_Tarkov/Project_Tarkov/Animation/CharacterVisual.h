#pragma once
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
    glm::vec3 positionOffset{0.0f};
};

class CharacterVisual
{
public:
    CharacterVisual() = default;
    ~CharacterVisual();
    CharacterVisual(const CharacterVisual&) = delete;
    CharacterVisual& operator=(const CharacterVisual&) = delete;
    void Init();
    void Update(float dt, const Character& character, const CharacterVisualState& state = {}, float poseInterval = 0.0f);
    void Render(const Character& character, Shader& shader, Camera& camera, float width, float height,
        const glm::vec3& color = glm::vec3(0.3f, 0.8f, 0.4f), bool firstPerson = false);
    void Reset();
    glm::vec3 FitLootPose(const Character& character, const Character& corpse,
        const CharacterVisual& corpseVisual,
        const std::function<bool(const glm::vec3&, float, const std::array<glm::vec3, 4>&)>& placementClear);
    bool LootPoseValid() const { return lootPoseValid; }
    float LootPoseHeight() const { return lootPoseHeight; }
    const std::array<glm::vec3, 4>& LootSupports() const { return lootSupports; }

private:
    void UploadPalette();
    struct Assets;
    std::shared_ptr<Assets> assets;
    GLuint paletteBuffer = 0, paletteTexture = 0;
    float poseElapsed = 0;
    bool lootPoseValid = false;
    float lootPoseHeight = .75f;
    std::array<glm::vec3, 4> lootSupports{};
    SkeletalAnimation animation;
    WeaponVisual weapon;
    glm::mat4 weaponModel{1}; // Character-local meters, shared by IK and rendering.
    MotionPose deathRoot;
    CharacterVisualState state;
    float deathTime = 0, holdWeight = 0;
    float motionClock = 0, aimWeight = 0, recoilTime = 1;
    std::array<float, 5> stanceWeights{};
    bool dying = false;
    bool firePending = false;
};
