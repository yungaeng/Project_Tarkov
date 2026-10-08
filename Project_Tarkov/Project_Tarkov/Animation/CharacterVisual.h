#pragma once
#include "../Graphics/Mesh.h"
#include "SkeletalAnimation.h"
#include "WeaponVisual.h"
#include <vector>

class Character;
class Camera;
class Shader;

struct CharacterVisualState
{
    bool dead = false, equipped = false, aiming = false, fired = false;
    float reloadRemaining = 0, aimPitch = 0;
};

class CharacterVisual
{
public:
    void Init();
    void Update(float dt, const Character& character, const CharacterVisualState& state = {});
    void Render(const Character& character, Shader& shader, Camera& camera, float width, float height,
        const glm::vec3& color = glm::vec3(0.3f, 0.8f, 0.4f));
    void Reset();

private:
    void CopyAnimatedVertices();
    SkeletalAnimation animation;
    Mesh mesh;
    std::vector<Vertex> vertices;
    MotionClip deathClip, holdClip, reloadClip;
    WeaponVisual weapon;
    MotionPose deathRoot;
    CharacterVisualState state;
    float deathTime = 0, holdWeight = 0;
    bool dying = false;
};
