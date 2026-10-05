#pragma once
#include "../Project_Tarkov/Project_Tarkov/Character.h"
#include <cassert>
#include <cmath>

inline void RunPhysicsTests()
{
    const auto near = [](float a, float b) { return std::abs(a - b) < 0.001f; };
    CollisionWorld world;
    world.AddBox({ { -10, -1, -10 }, { 10, 0, 10 } });
    Character falling;
    falling.SetCollisionWorld(&world);
    falling.position = { 0, 3, 0 };
    falling.Update(0.1f);
    assert(falling.position.y < 3 && !falling.IsGrounded());
    assert(falling.GetVerticalVelocity() < 0);
    falling.Update(1);
    assert(near(falling.position.y, 0) && falling.IsGrounded());
    assert(near(falling.GetVerticalVelocity(), 0));
    for (int i = 0; i < 120; ++i) falling.Update(1.0f / 60);
    assert(near(falling.position.y, 0) && falling.IsGrounded());

    // A thin wall must stop both slow and fast motion, while allowing sliding.
    world.AddBox({ { 2, 0, -8 }, { 2.01f, 5, 8 } });
    falling.SetMovement({ 1, 0, 1 }, 9);
    falling.Update(0.5f);
    assert(near(falling.position.x, 2 - Character::BodyRadius));
    assert(falling.position.z > 3 && falling.IsGrounded());
    Character fast;
    fast.SetCollisionWorld(&world);
    fast.SetMovement({ 1, 0, 0 }, 500);
    fast.Update(0.1f);
    assert(near(fast.position.x, 2 - Character::BodyRadius));

    // Reverse movement and wall contact must not stick.
    fast.SetMovement({ -1, 0, 0 }, 5);
    fast.Update(0.1f);
    assert(fast.position.x < 1.2f);
    fast.StopMovement();
    fast.position = { 3, 0, 0 };
    fast.SetMovement({ -1, 0, 0 }, 50);
    fast.Update(0.1f);
    assert(near(fast.position.x, 2.01f + Character::BodyRadius));

    // Finite ground: walking off the edge begins a fall, even while touching a wall.
    CollisionWorld ledge;
    ledge.AddBox({ { -1, -1, -1 }, { 1, 0, 1 } });
    Character walker;
    walker.SetCollisionWorld(&ledge);
    walker.SetMovement({ 1, 0, 0 }, 5);
    walker.Update(0.5f);
    assert(!walker.IsGrounded() && walker.position.y < 0);

    // Elevated platforms count as ground; objects overhead do not.
    CollisionWorld platform;
    platform.AddBox({ { -1, 1, -1 }, { 1, 2, 1 } });
    Character landing;
    landing.SetCollisionWorld(&platform);
    landing.position = { 0, 5, 0 };
    landing.Update(1);
    assert(near(landing.position.y, 2) && landing.IsGrounded());
    glm::vec3 feet = { 0, -2, 0 };
    assert(platform.MoveAxis(feet, Character::BodyRadius, Character::BodyHeight, 1, 5));
    assert(near(feet.y, 1 - Character::BodyHeight));
    assert(!platform.IsSupported(feet, Character::BodyRadius, Character::BodyHeight));

    // Recover a small overlap with the floor on spawn.
    Character embedded;
    embedded.SetCollisionWorld(&ledge);
    embedded.position = { 0, -0.05f, 0 };
    embedded.Update(0.01f);
    assert(near(embedded.position.y, 0) && embedded.IsGrounded());

    // Gravity continues without movement input; inactive characters remain frozen.
    Character freeFall;
    freeFall.StopMovement();
    freeFall.Update(0.1f);
    assert(freeFall.position.y < 0 && !freeFall.IsGrounded());
    const float lastY = freeFall.position.y;
    freeFall.active = false;
    freeFall.Update(1);
    assert(freeFall.position.y == lastY);

    // Frame-rate variation should have only the small integration error of one substep.
    Character a, b;
    a.Update(1);
    for (int i = 0; i < 60; ++i) b.Update(1.0f / 60);
    assert(std::abs(a.position.y - b.position.y) < 0.05f);
    assert(near(a.GetVerticalVelocity(), b.GetVerticalVelocity()));
}
