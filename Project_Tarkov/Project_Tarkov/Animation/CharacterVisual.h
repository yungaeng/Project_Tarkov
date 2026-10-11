#pragma once
#include "../Graphics/Mesh.h"
#include "SkeletalAnimation.h"
#include "WeaponVisual.h"
#include <memory>
#include <array>

class Character;
class Camera;
class Shader;

struct CharacterVisualState
{
    bool dead = false, equipped = false, aiming = false, fired = false;
    float reloadRemaining = 0, aimPitch = 0;
    bool hasWeapon = true;
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
        const glm::vec3& color = glm::vec3(0.3f, 0.8f, 0.4f));
    void Reset();

private:
    void UploadPalette();
    struct Assets;
    std::shared_ptr<Assets> assets;
    GLuint paletteBuffer = 0, paletteTexture = 0;
    float poseElapsed = 0;
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
