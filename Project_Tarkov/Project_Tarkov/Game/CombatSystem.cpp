#include "CombatSystem.h"
#include "IndustrialZone.h"
#include "Player.h"
#include "LootSystem.h"
#include "../Core/Input.h"
#include "../Physics/CollisionWorld.h"
#include "../Physics/Raycast.h"
#include "../Graphics/Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace
{
    glm::vec3 Eye(const Character& body)
    {
        return body.position + glm::vec3(0, body.IsCrouching() ? body.GetSettings().crouchingEyeHeight : body.GetSettings().standingEyeHeight, 0);
    }
    bool Visible(const CollisionWorld& world, glm::vec3 from, glm::vec3 to)
    {
        const auto delta = to - from;
        const float length = glm::length(delta);
        if (length < 0.001f) return true;
        float hit;
        for (const auto& box : world.GetBoxes())
            if (RayBox(from, delta / length, box.min, box.max, length, hit)) return false;
        return true;
    }
    bool HitBody(const Character& body, glm::vec3 origin, glm::vec3 direction, float limit, float& hit)
    {
        const float radius = body.GetSettings().bodyRadius;
        return RayBox(origin, direction, body.position - glm::vec3(radius, 0, radius),
            body.position + glm::vec3(radius, body.GetBodyHeight(), radius), limit, hit);
    }
}

void CombatSystem::Init(const CollisionWorld& world)
{
    enemies.clear();
    for (const glm::vec3 position : IndustrialZone::Enemies)
    {
        Enemy enemy;
        enemy.body.SetCollisionWorld(&world);
        enemy.body.position = enemy.home = enemy.lastSeen = position;
        enemy.visual = std::make_unique<CharacterVisual>();
        enemy.visual->Init();
        enemies.push_back(std::move(enemy));
    }
    magazine = 30;
    hasRifle = equipped = true;
    aiming = false;
    fired = false;
    cooldown = reload = messageTime = 0;
    message.clear();
}

int CombatSystem::LivingEnemies() const
{
    int count = 0;
    for (const auto& enemy : enemies) if (enemy.health > 0) ++count;
    return count;
}

void CombatSystem::Update(float dt, Player& player, Camera& camera, const CollisionWorld& world, LootSystem& loot, bool enabled)
{
    fired = false;
    cooldown = (std::max)(0.0f, cooldown - dt);
    messageTime = (std::max)(0.0f, messageTime - dt);
    if (messageTime == 0) message.clear();
    enabled = enabled && player.vitals.Alive();
    if (!enabled) reload = 0;
    if (enabled && hasRifle && Input::GetKeyDown(GLFW_KEY_1)) { equipped = !equipped; reload = 0; }
    aiming = enabled && equipped && Input::Mouse(GLFW_MOUSE_BUTTON_RIGHT);
    camera.fieldOfView = aiming ? 50.0f : 75.0f;
    if (reload > 0)
    {
        reload = (std::max)(0.0f, reload - dt);
        if (reload == 0) magazine += player.GetInventory().Take(ItemType::Ammo, 30 - magazine);
    }
    if (enabled && equipped && Input::GetKeyDown(GLFW_KEY_R) && reload == 0)
    {
        if (magazine < 30 && player.GetInventory().Count(ItemType::Ammo) > 0) reload = 2;
        else { message = "Magazine full or no reserve ammunition"; messageTime = 2; }
    }
    if (enabled && equipped && reload == 0 && cooldown == 0 && Input::MouseDown(GLFW_MOUSE_BUTTON_LEFT))
    {
        if (magazine == 0) { message = "Empty - press R to reload"; messageTime = 2; }
        else
        {
            --magazine;
            fired = true;
            cooldown = 0.15f;
            // Camera selects an aim point; the second ray starts at the player's eye,
            // so the third-person camera cannot shoot around intervening cover.
            float nearest = 100, hit = 0;
            for (const auto& box : world.GetBoxes())
                if (RayBox(camera.position, camera.front, box.min, box.max, nearest, hit)) nearest = hit;
            for (const auto& enemy : enemies)
                if (enemy.health > 0 && HitBody(enemy.body, camera.position, camera.front, nearest, hit)) nearest = hit;
            const auto origin = Eye(player);
            const auto delta = camera.position + camera.front * nearest - origin;
            const float length = glm::length(delta);
            int victim = -1;
            if (length > 0.001f && glm::dot(delta, camera.front) > 0)
            {
                const auto direction = delta / length;
                nearest = length + 0.01f;
                bool wall = false;
                for (const auto& box : world.GetBoxes())
                    if (RayBox(origin, direction, box.min, box.max, nearest, hit)) { nearest = hit; wall = true; }
                for (std::size_t i = 0; i < enemies.size(); ++i)
                    if (enemies[i].health > 0 && HitBody(enemies[i].body, origin, direction, nearest, hit) && hit < nearest)
                    { nearest = hit; victim = static_cast<int>(i); }
                message = victim >= 0 ? "Hit" : wall ? "Hit cover" : "Miss";
            }
            else message = "Aim blocked by cover";
            if (victim >= 0)
            {
                auto& enemy = enemies[static_cast<std::size_t>(victim)];
                enemy.health = (std::max)(0.0f, enemy.health - 34);
                enemy.lastSeen = player.position;
                enemy.state = enemy.health == 0 ? EnemyState::Dead : EnemyState::Chase;
                enemy.searchTimer = 5;
                if (enemy.health == 0) message = "Enemy killed";
            }
            messageTime = 1;
            camera.pitch = std::clamp(camera.pitch + (aiming ? 0.7f : 1.6f), -89.0f, 89.0f);
            camera.UpdateDirection();
        }
    }

    for (auto& enemy : enemies)
    {
        enemy.fired = false;
        enemy.body.StopMovement();
        if (enemy.health <= 0)
        {
            enemy.state = EnemyState::Dead;
            enemy.body.Update(dt);
            if (!enemy.lootDropped && enemy.body.IsGrounded())
            {
                // Mark each corpse only after a successful allocation/spawn.
                loot.Spawn(enemy.body.position + glm::vec3(0, WorldItem::HalfSize, 0), {ItemType::Ammo, 20});
                enemy.lootDropped = true;
            }
            continue;
        }
        const float distance = glm::length(player.position - enemy.body.position);
        const bool seesPlayer = player.vitals.Alive() && distance < 18 && Visible(world, Eye(enemy.body), Eye(player));
        enemy.attackTimer = (std::max)(0.0f, enemy.attackTimer - dt);
        if (seesPlayer)
        {
            enemy.lastSeen = player.position;
            enemy.searchTimer = 5;
            enemy.state = distance < 11 ? EnemyState::Attack : EnemyState::Chase;
        }
        else if (enemy.state == EnemyState::Attack || enemy.state == EnemyState::Chase) enemy.state = EnemyState::Search;
        if (enemy.state == EnemyState::Search)
        {
            enemy.searchTimer -= dt;
            if (enemy.searchTimer <= 0) enemy.state = EnemyState::Patrol;
        }
        if (enemy.state == EnemyState::Attack)
        {
            if (enemy.attackTimer == 0 && seesPlayer)
            {
                player.vitals.Damage(8);
                enemy.fired = true;
                enemy.attackTimer = 1.2f;
            }
        }
        else
        {
            glm::vec3 target = enemy.lastSeen;
            if (enemy.state == EnemyState::Patrol)
            {
                target = enemy.home + (enemy.returning ? glm::vec3(0) : glm::vec3(4, 0, 0));
                if (glm::length(target - enemy.body.position) < 0.5f) enemy.returning = !enemy.returning;
            }
            glm::vec3 direction = target - enemy.body.position;
            direction.y = 0;
            if (glm::length(direction) > 0.5f)
            {
                direction = glm::normalize(direction);
                // Local avoidance chooses a clear short step along either side of cover.
                const float step = 0.8f;
                if (!world.CanOccupy(enemy.body.position + direction * step, 0.35f, enemy.body.GetBodyHeight()))
                {
                    const glm::vec3 side(-direction.z, 0, direction.x);
                    if (world.CanOccupy(enemy.body.position + side * step, 0.35f, enemy.body.GetBodyHeight())) direction = side;
                    else if (world.CanOccupy(enemy.body.position - side * step, 0.35f, enemy.body.GetBodyHeight())) direction = -side;
                    else direction = glm::vec3(0);
                }
                enemy.body.SetMovement(direction, enemy.state == EnemyState::Patrol ? 1.4f : 2.8f);
            }
        }
        enemy.body.Update(dt);
        if (seesPlayer)
        {
            const auto direction = Eye(player) - Eye(enemy.body);
            enemy.body.rotation.y = glm::degrees(std::atan2(direction.x, direction.z));
            enemy.aimPitch = glm::degrees(std::atan2(direction.y, glm::length(glm::vec2(direction.x, direction.z))));
        }
    }
    for (auto& enemy : enemies)
    {
        const auto delta = enemy.body.position - camera.position;
        const float distanceSquared = glm::dot(delta, delta);
        // Only visual poses are throttled. AI, physics and weapon events stay per-frame.
        const float interval = distanceSquared > 60 * 60 ? 1.0f / 15.0f :
            distanceSquared > 25 * 25 ? 1.0f / 30.0f : 0.0f;
        enemy.visual->Update(dt, enemy.body, {enemy.health <= 0, true, enemy.state == EnemyState::Attack,
            enemy.fired, 0, enemy.aimPitch}, interval);
    }
    if (!player.vitals.Alive()) { reload = 0; aiming = false; camera.fieldOfView = 75; }
}

void CombatSystem::Render(Shader& shader, Camera& camera, float width, float height)
{
    for (auto& enemy : enemies)
    {
        enemy.visual->Render(enemy.body, shader, camera, width, height, glm::vec3(0.72f, 0.43f, 0.28f));
    }
}
