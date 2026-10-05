#include "../Project_Tarkov/Project_Tarkov/Player.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include "physics_tests.h"

static bool Near(float a, float b)
{
    return std::abs(a - b) < 0.0001f;
}

int main()
{
    Player player;
    Entity& entity = player;
    // The renderer, Entity reference, and movement all use one transform.
    assert(&entity.position == &player.position);
    entity.position = { 1, 2, 3 };
    player.SetMovement({ 0, 0, -1 }, 5);
    entity.Update(0.5f);
    assert(Near(player.position.z, 0.5f) && player.position.y < 2);

    player.position = { 0, 0, 0 };
    player.SetMovement({ 1, 20, 1 }, 5);
    entity.Update(1);
    assert(Near(std::sqrt(player.position.x * player.position.x + player.position.z * player.position.z), 5));
    assert(player.position.y < 0); // Gravity is independent of the input's vertical component.

    player.StopMovement();
    const glm::vec3 stopped = player.position;
    entity.Update(1);
    assert(Near(player.position.x, stopped.x) && Near(player.position.z, stopped.z));
    player.SetMovement({ 0, 0, 0 }, 5);
    entity.Update(1);
    assert(Near(player.position.x, stopped.x));

    player.position = { 0, 0, 0 };
    player.SetMovement({ 1, 0, 0 }, 9);
    entity.Update(1);
    assert(Near(player.position.x, 9));
    player.SetMovement({ 1, 0, 0 }, 2.5f);
    entity.Update(1);
    assert(Near(player.position.x, 11.5f));

    entity.active = false;
    entity.Update(1);
    assert(Near(player.position.x, 11.5f));
    entity.active = true;
    entity.Update(0);
    entity.Update(-1);
    entity.Update(std::numeric_limits<float>::quiet_NaN());
    assert(Near(player.position.x, 11.5f));

    player.SetMovement({ 1, 0, 0 }, -1);
    entity.Update(1);
    player.SetMovement({ std::numeric_limits<float>::infinity(), 0, 0 }, 5);
    entity.Update(1);
    assert(Near(player.position.x, 11.5f));

    // Any future AI subclass uses exactly the same movement implementation.
    struct TestAI : Character {};
    TestAI ai;
    Player other;
    ai.SetMovement({ 0, 0, 1 }, 5);
    other.SetMovement({ 0, 0, 1 }, 5);
    for (int i = 0; i < 10; ++i) ai.Update(0.1f);
    other.Update(1);
    assert(Near(ai.position.z, other.position.z));
    RunPhysicsTests();
    std::cout << "Character and physics regression tests passed\n";
}
