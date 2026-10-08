#pragma once
#include "MotionClip.h"
#include "../Graphics/Mesh.h"
#include <memory>
class Camera;
class Shader;

// Shared immutable meshes/clips; each actor owns its own animation clocks.
class WeaponVisual
{
public:
    void Init();
    void Update(float dt, bool equipped, bool aiming, float reloadRemaining, bool fired, bool moving, bool dead, bool sprinting = false, bool crouching = false);
    void Render(const glm::mat4& grip, Shader& shader, Camera& camera, float width, float height);
    void Reset();
private:
    struct Assets;
    std::shared_ptr<Assets> assets;
    float clock = 0, recoilTime = 10, equipTime = 0, reloadTime = -1, aimWeight = 0, moveWeight = 0;
    float sprintWeight = 0, crouchWeight = 0;
    bool visible = false, wasEquipped = false, dead = false;
};
