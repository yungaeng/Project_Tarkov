#pragma once
#include "../Graphics/Mesh.h"
#include "SkeletalAnimation.h"
#include "WeaponVisual.h"
#include <memory>

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
    MotionPose deathRoot;
    CharacterVisualState state;
    float deathTime = 0, holdWeight = 0;
    bool dying = false;
};
