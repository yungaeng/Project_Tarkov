#pragma once
#include "Entity.h"
#include "CharacterSettings.h"

class CollisionWorld;

// Shared movement for player and future AI characters.
// Controllers supply world-space intent; characters do not read keyboard or camera state.
class Character : public Entity
{
private:
    glm::vec3 moveDirection = { 0, 0, 0 };
    float moveSpeed = 0.0f;
    const CollisionWorld* collisionWorld = nullptr; // Owned by the scene; outlives this character.
    float verticalVelocity = 0.0f;
    bool grounded = false;
    bool crouching = false;
    bool sprinting = false;
    bool moving = false;
    CharacterSettings settings;

public:
    Character() { rotation.y = -90.0f; }
    const CharacterSettings& GetSettings() const { return settings; }
    bool IsCrouching() const { return crouching; }
    bool IsSprinting() const { return sprinting; }
    bool IsMoving() const { return moving; }
    float GetBodyHeight() const { return crouching ? settings.crouchingBodyHeight : settings.bodyHeight; }
    // A standing request is rejected when the full standing body would overlap geometry.
    // Controllers may repeat the request each frame to stand as soon as space is available.
    void SetCrouching(bool value);
    void SetSprinting(bool value) { sprinting = value && !crouching; }

    void SetCollisionWorld(const CollisionWorld* world);
    bool IsGrounded() const { return grounded; }
    float GetVerticalVelocity() const { return verticalVelocity; }

    void SetMovement(const glm::vec3& direction, float speed);

    void StopMovement();

    void Update(float dt) override;
};
