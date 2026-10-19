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
#include <iomanip>
#include <sstream>

namespace
{
    glm::vec3 Eye(const Character& body)
    {
        if (body.IsLooting()) {
            const float yaw = glm::radians(body.rotation.y);
            return body.position + glm::vec3(std::sin(yaw)*0.7f, 1.0f, std::cos(yaw)*0.7f);
        }
        return body.position + glm::vec3(0, body.GetEyeHeight(), 0);
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
        if (body.IsLooting()) {
            const float yaw = glm::radians(body.rotation.y);
            const glm::vec3 forward(std::sin(yaw), 0, std::cos(yaw));
            const auto center = body.position + forward * .55f + glm::vec3(0, .6f, 0);
            const auto extent = glm::abs(forward) * .65f + glm::vec3(.35f, .6f, .35f);
            return RayBox(origin, direction, center - extent, center + extent, limit, hit);
        }
        const float radius = body.GetSettings().bodyRadius;
        return RayBox(origin, direction, body.position - glm::vec3(radius, 0, radius),
            body.position + glm::vec3(radius, body.GetBodyHeight(), radius), limit, hit);
    }
}

void CombatSystem::Init(const CollisionWorld& world)
{
    gunshotAudio.Init();
    enemies.clear();
    kills.clear();
    for (const glm::vec3 position : IndustrialZone::Enemies)
    {
        Enemy enemy;
        enemy.body.SetCollisionWorld(&world);
        enemy.body.position = enemy.home = enemy.lastSeen = position;
        enemy.inventory.TryAdd({ItemType::Ammo, 20});
        enemy.inventory.TryAdd({ItemType::Bandage, 1});
        enemy.inventory.TryAdd({ItemType::Water, 1});
        enemy.visual = std::make_unique<CharacterVisual>();
        enemy.visual->Init();
        enemies.push_back(std::move(enemy));
    }
    magazine = 30;
    hasRifle = equipped = true;
    aiming = false;
    fired = false;
    cooldown = reload = messageTime = elapsedSeconds = 0;
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
    elapsedSeconds += dt;
    fired = false;
    cooldown = (std::max)(0.0f, cooldown - dt);
    messageTime = (std::max)(0.0f, messageTime - dt);
    if (messageTime == 0) message.clear();
    enabled = enabled && player.vitals.Alive();
    if (!enabled) reload = 0;
    if (enabled && hasRifle && Input::GetKeyDown(GLFW_KEY_1)) { equipped = !equipped; reload = 0; }
    if (reload > 0)
    {
        reload = (std::max)(0.0f, reload - dt);
        if (reload == 0) magazine += player.GetInventory().Take(ItemType::Ammo, 30 - magazine);
    }
    if (enabled && equipped && Input::GetKeyDown(GLFW_KEY_R) && reload == 0)
    {
        if (magazine < 30 && player.GetInventory().Count(ItemType::Ammo) > 0) reload = 2;
        else { message = "탄창이 가득 찼거나 예비 탄약이 없습니다"; messageTime = 2; }
    }
    aiming = enabled && equipped && reload == 0 && Input::Mouse(GLFW_MOUSE_BUTTON_RIGHT);
    camera.fieldOfView = aiming ? 50.0f : 75.0f;
    if (enabled && equipped && reload == 0 && cooldown == 0 && Input::MouseDown(GLFW_MOUSE_BUTTON_LEFT))
    {
        if (magazine == 0) { message = "탄약 소진 - R키로 재장전하세요"; messageTime = 2; }
        else
        {
            --magazine;
            fired = true;
            gunshotAudio.Play();
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
            glm::vec3 impactPosition(0.0f);
            if (length > 0.001f && glm::dot(delta, camera.front) > 0)
            {
                const auto direction = delta / length;
                nearest = length + 0.01f;
                bool wall = false;
                for (const auto& box : world.GetBoxes())
                    if (RayBox(origin, direction, box.min, box.max, nearest, hit)) { nearest = hit; wall = true; }
                for (std::size_t i = 0; i < enemies.size(); ++i)
                    if (enemies[i].health > 0 && HitBody(enemies[i].body, origin, direction, nearest, hit) && hit < nearest)
                    { nearest = hit; victim = static_cast<int>(i); impactPosition = origin + direction * hit; }
                message = victim >= 0 ? "명중" : wall ? "엄폐물에 명중" : "빗나감";
            }
            else message = "엄폐물이 사격을 가로막고 있습니다";
            if (victim >= 0)
            {
                auto& enemy = enemies[static_cast<std::size_t>(victim)];
                enemy.health = (std::max)(0.0f, enemy.health - 34);
                enemy.lastSeen = player.position;
                enemy.state = enemy.health == 0 ? EnemyState::Dead : EnemyState::Chase;
                enemy.searchTimer = 5;
                if (enemy.health == 0) {
                    for (int slot = 0; slot < ClothingSlotCount; ++slot) {
                        const auto type = ClothingType(slot);
                        if (enemy.body.clothingMask & ClothingBit(type)) enemy.inventory.TryAdd({type, 1});
                    }
                    message = "적 처치";
                    const int killSeconds = static_cast<int>(elapsedSeconds);
                    std::ostringstream timestamp;
                    timestamp << std::setfill('0') << std::setw(2) << killSeconds / 60
                        << ':' << std::setw(2) << killSeconds % 60;
                    std::ostringstream location;
                    location << "X " << static_cast<int>(std::round(enemy.body.position.x))
                        << " / Z " << static_cast<int>(std::round(enemy.body.position.z));
                    kills.push_back({
                        "적 #" + std::to_string(victim + 1),
                        timestamp.str(),
                        location.str(),
                        impactPosition.y - enemy.body.position.y > enemy.body.GetBodyHeight() * 0.75f ? "머리" : "몸통"
                    });
                }
            }
            messageTime = 1;
            camera.pitch = std::clamp(camera.pitch + (aiming ? 0.7f : 1.6f), -89.0f, 89.0f);
            camera.UpdateDirection();
        }
    }

    for (std::size_t enemyIndex = 0; enemyIndex < enemies.size(); ++enemyIndex)
    {
        auto& enemy = enemies[enemyIndex];
        enemy.fired = false;
        enemy.body.StopMovement();
        if (enemy.health <= 0)
        {
            enemy.state = EnemyState::Dead;
            enemy.body.Update(dt);
            enemy.deadTime += dt;
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
                const int hitPart = (static_cast<int>(enemyIndex) + static_cast<int>(elapsedSeconds)) % BodyPartCount;
                player.vitals.Damage(static_cast<BodyPart>(hitPart), 40);
                enemy.fired = true;
                if (Input::IsFocused()) {
                    const auto offset = Eye(enemy.body) - camera.position;
                    const float soundDistance = glm::length(offset);
                    const auto right = glm::cross(camera.front, glm::vec3(0, 1, 0));
                    const float rightLength = glm::length(right);
                    const float pan = soundDistance > .001f && rightLength > .001f ?
                        glm::dot(offset / soundDistance, right / rightLength) : 0.f;
                    gunshotAudio.Play(.55f / (1.f + soundDistance * soundDistance / 100.f), pan);
                }
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
    // Conservative horizontal body volumes prevent living actors from walking
    // through one another or through settled corpses. Static geometry still wins.
    auto centerOf = [](const Character& body, bool dead) {
        const float yaw = glm::radians(body.rotation.y);
        return body.position + glm::vec3(std::sin(yaw), 0, std::cos(yaw)) *
            (dead ? -1.1f : body.IsLooting() ? .9f : 0.f);
    };
    auto separate = [&](Character& moving, const Character& obstacle, bool corpse) {
        if (std::abs(moving.position.y - obstacle.position.y) > 1.f) return;
        const auto delta = centerOf(moving, false) - centerOf(obstacle, corpse);
        const glm::vec2 horizontal(delta.x, delta.z);
        const float distance = glm::length(horizontal);
        const float gap = .36f + (corpse ? 1.12f : obstacle.IsLooting() ? 1.05f : .36f);
        if (distance >= gap) return;
        const glm::vec2 normal = distance > .001f ? horizontal / distance : glm::vec2(1, 0);
        const auto next = moving.position + glm::vec3(normal.x, 0, normal.y) * (gap - distance + .01f);
        if (world.CanOccupy(next, moving.GetSettings().bodyRadius, moving.GetBodyHeight())) {
            moving.position = next;
            moving.StopMovement();
        }
    };
    for (int pass = 0; pass < 3; ++pass) {
        for (std::size_t i = 0; i < enemies.size(); ++i) {
            auto& enemy = enemies[i];
            if (enemy.health > 0) {
                separate(enemy.body, player, false);
                for (std::size_t j = 0; j < enemies.size(); ++j)
                    if (i != j) separate(enemy.body, enemies[j].body, enemies[j].health <= 0);
            }
            if (!player.IsLooting() && player.vitals.Alive())
                separate(player, enemy.body, enemy.health <= 0);
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
            enemy.fired, 0, enemy.aimPitch, enemy.rifle}, interval);
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

const Enemy* CombatSystem::Corpse(int index) const
{
    if (index < 0 || index >= static_cast<int>(enemies.size())) return nullptr;
    const auto& enemy = enemies[static_cast<std::size_t>(index)];
    return enemy.health <= 0 && enemy.deadTime >= 1.25f && enemy.body.IsGrounded() ? &enemy : nullptr;
}

int CombatSystem::FindCorpse(const Player& player, const CollisionWorld& world) const
{
    // Searching uses a taller kneeling pose; stand up before entering it.
    if (player.IsProne()) return -1;
    float nearest = 2.5f;
    int selected = -1;
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i) {
        const auto* corpse = Corpse(i);
        if (!corpse) continue;
        const float yaw = glm::radians(corpse->body.rotation.y);
        const auto center = corpse->body.position - glm::vec3(std::sin(yaw), 0, std::cos(yaw)) * 1.1f;
        const float distance = glm::length(center - player.position);
        if (distance < nearest && Visible(world, Eye(player), center + glm::vec3(0, .3f, 0))) {
            nearest = distance;
            selected = i;
        }
    }
    return selected;
}

bool CombatSystem::TakeCorpseItem(int index, StackId id, Player& player)
{
    if (!Corpse(index) || !player.IsLooting() || !player.vitals.Alive()) return false;
    auto& enemy = enemies[static_cast<std::size_t>(index)];
    const auto* entry = enemy.inventory.Find(id);
    if (!entry) return false;
    const ItemStack stack = *entry;
    if (!player.GetInventory().TryAdd(stack)) return false;
    enemy.inventory.Remove(id, stack.quantity);
    if (IsClothing(stack.type)) enemy.body.clothingMask &= ~ClothingBit(stack.type);
    return true;
}

bool CombatSystem::TakeCorpseRifle(int index)
{
    if (!Corpse(index) || hasRifle) return false;
    auto& enemy = enemies[static_cast<std::size_t>(index)];
    if (!enemy.rifle) return false;
    enemy.rifle = false;
    SetLoadout(true, 30);
    return true;
}

bool CombatSystem::LootSpaceClear(const glm::vec3& center, int ignoredCorpse) const
{
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i) {
        if (i == ignoredCorpse) continue;
        const auto& enemy = enemies[static_cast<std::size_t>(i)];
        const float yaw = glm::radians(enemy.body.rotation.y);
        const auto other = enemy.body.position - (enemy.health <= 0 ? 1.1f : 0.f) *
            glm::vec3(std::sin(yaw), 0, std::cos(yaw));
        if (std::abs(center.y - other.y) < 1.f &&
            glm::length(glm::vec2(center.x-other.x, center.z-other.z)) < (enemy.health <= 0 ? 2.2f : 1.4f)) return false;
    }
    return true;
}
