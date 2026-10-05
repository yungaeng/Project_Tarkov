#pragma once
#include "Entity.h"
#include "CollisionWorld.h"
#include <glm/geometric.hpp>
#include <cmath>

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

public:
    static constexpr float BodyRadius = 0.35f;
    static constexpr float BodyHeight = 1.8f;
    static constexpr float Gravity = 9.81f;
    static constexpr float TerminalSpeed = 50.0f;

    void SetCollisionWorld(const CollisionWorld* world)
    {
        collisionWorld = world;
        verticalVelocity = 0;
        grounded = false;
    }
    bool IsGrounded() const { return grounded; }
    float GetVerticalVelocity() const { return verticalVelocity; }

    void SetMovement(const glm::vec3& direction, float speed)
    {
        StopMovement();
        if (!std::isfinite(direction.x) || !std::isfinite(direction.z) ||
            !std::isfinite(speed) || speed <= 0.0f)
            return;

        // Ground movement does not inherit the camera's vertical direction.
        const glm::vec3 horizontal = { direction.x, 0, direction.z };
        const float length = glm::length(horizontal);
        if (!std::isfinite(length) || length <= 0.0f)
            return;

        moveDirection = horizontal / length;
        moveSpeed = speed;
    }

    void StopMovement()
    {
        moveDirection = { 0, 0, 0 };
        moveSpeed = 0.0f;
    }

    void Update(float dt) override
    {
        if (!active || !std::isfinite(dt) || dt <= 0.0f)
            return;

        // Bound work after a debugger pause; substeps improve diagonal and edge behaviour.
        const float elapsed = (std::min)(dt, 1.0f);
        const int steps = static_cast<int>(std::ceil(elapsed * 120.0f));
        const float step = elapsed / steps;
        if (collisionWorld)
            collisionWorld->ResolveOverlap(position, BodyRadius, BodyHeight);
        for (int i = 0; i < steps; ++i)
        {
            const float dx = moveDirection.x * moveSpeed * step;
            const float dz = moveDirection.z * moveSpeed * step;
            verticalVelocity = (std::max)(verticalVelocity - Gravity * step, -TerminalSpeed);
            if (collisionWorld)
            {
                collisionWorld->MoveAxis(position, BodyRadius, BodyHeight, 0, dx);
                collisionWorld->MoveAxis(position, BodyRadius, BodyHeight, 2, dz);
                if (collisionWorld->MoveAxis(position, BodyRadius, BodyHeight, 1, verticalVelocity * step))
                    verticalVelocity = 0;
                grounded = collisionWorld->IsSupported(position, BodyRadius, BodyHeight);
                if (grounded && verticalVelocity < 0) verticalVelocity = 0;
            }
            else
            {
                position.x += dx;
                position.z += dz;
                position.y += verticalVelocity * step;
                grounded = false;
            }
        }
    }
};
