#pragma once
#include "Entity.h"
#include "CharacterSettings.h"
#include "Inventory.h"

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
    bool prone = false;
    bool sprinting = false;
    bool moving = false;
    bool looting = false;
    CharacterSettings settings;

public:
    std::uint32_t clothingMask = DefaultClothingMask;
    Character() { rotation.y = -90.0f; }
    const CharacterSettings& GetSettings() const { return settings; }
    bool IsCrouching() const { return crouching; }
    bool IsProne() const { return prone; }
    bool SetProne(bool value);
    float GetEyeHeight() const { return prone ? settings.proneEyeHeight : crouching ? settings.crouchingEyeHeight : settings.standingEyeHeight; }
    bool IsSprinting() const { return sprinting; }
    bool IsMoving() const { return moving; }
    bool IsLooting() const { return looting; }
    void SetLooting(bool value) { looting = value; if (value) { StopMovement(); sprinting = false; } }
    float GetBodyHeight() const { return looting ? 1.2f : prone ? settings.proneBodyHeight : crouching ? settings.crouchingBodyHeight : settings.bodyHeight; }
    // A standing request is rejected when the full standing body would overlap geometry.
    // Controllers may repeat the request each frame to stand as soon as space is available.
    void SetCrouching(bool value);
    void SetSprinting(bool value) { sprinting = value && !crouching && !prone; }

    void SetCollisionWorld(const CollisionWorld* world);
    bool IsGrounded() const { return grounded; }
    float GetVerticalVelocity() const { return verticalVelocity; }
    bool Jump();

    void SetMovement(const glm::vec3& direction, float speed);

    void StopMovement();

    void Update(float dt) override;
};
