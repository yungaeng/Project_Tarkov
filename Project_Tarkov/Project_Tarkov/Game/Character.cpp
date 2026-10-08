#include "Character.h"
#include "../Physics/CollisionWorld.h"
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <algorithm>
#include <cmath>

void Character::SetCrouching(bool value)
{
    if (!value && crouching && collisionWorld &&
        !collisionWorld->CanOccupy(position, settings.bodyRadius, settings.bodyHeight))
        return;

    // Feet stay fixed when changing stance, including in the air.
    crouching = value;
    if (crouching) sprinting = false;
}

void Character::SetCollisionWorld(const CollisionWorld* world)
{
    collisionWorld = world;
    verticalVelocity = 0;
    grounded = false;
}

void Character::SetMovement(const glm::vec3& direction, float speed)
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

void Character::StopMovement()
{
    moveDirection = { 0, 0, 0 };
    moveSpeed = 0.0f;
}

void Character::Update(float dt)
{
    moving = false;
    if (!active || !std::isfinite(dt) || dt <= 0.0f)
        return;

    // Bound work after a debugger pause; substeps improve diagonal and edge behaviour.
    const glm::vec3 previousPosition = position;
    const float elapsed = (std::min)(dt, 1.0f);
    const int steps = static_cast<int>(std::ceil(elapsed * 120.0f));
    const float step = elapsed / steps;
    const float bodyHeight = GetBodyHeight();
    if (collisionWorld)
        collisionWorld->ResolveOverlap(position, settings.bodyRadius, bodyHeight);
    for (int i = 0; i < steps; ++i)
    {
        const float dx = moveDirection.x * moveSpeed * step;
        const float dz = moveDirection.z * moveSpeed * step;
        verticalVelocity = (std::max)(verticalVelocity - settings.gravity * step, -settings.terminalSpeed);
        if (collisionWorld)
        {
            collisionWorld->MoveAxis(position, settings.bodyRadius, bodyHeight, 0, dx);
            collisionWorld->MoveAxis(position, settings.bodyRadius, bodyHeight, 2, dz);
            if (collisionWorld->MoveAxis(position, settings.bodyRadius, bodyHeight, 1, verticalVelocity * step))
                verticalVelocity = 0;
            grounded = collisionWorld->IsSupported(position, settings.bodyRadius, bodyHeight);
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
    const glm::vec3 displacement = position - previousPosition;
    moving = grounded && glm::length(glm::vec3(displacement.x, 0, displacement.z)) > 0.00001f;
    if (moving) rotation.y = glm::degrees(std::atan2(displacement.x, displacement.z));
}
