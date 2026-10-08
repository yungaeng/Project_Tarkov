#include "LootSystem.h"
#include "IndustrialZone.h"
#include <random>
#include "Player.h"
#include "../Graphics/Renderer.h"
#include "../Physics/CollisionWorld.h"
#include "../Physics/Raycast.h"
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace
{
    bool Blocked(const CollisionWorld& world, glm::vec3 origin, glm::vec3 direction, float limit)
    {
        float distance = 0;
        for (const auto& box : world.GetBoxes())
            if (RayBox(origin, direction, box.min, box.max, limit, distance)) return true;
        return false;
    }
}

void LootSystem::Init()
{
    Reset();
    std::mt19937 random(std::random_device{}());
    std::uniform_int_distribution<int> kind(0, 2);
    // Four reachable floor pickups per building; existing MVP item definitions.
    for (const auto& building : IndustrialZone::Buildings)
        for (int point = 0; point < RaidConfig::MvpLootCount / 5; ++point) {
            const auto type = static_cast<ItemType>(kind(random));
            Spawn(building + glm::vec3(point % 2 ? 5.0f : -5.0f, WorldItem::HalfSize,
                point / 2 ? 8.0f : -8.0f), {type, type == ItemType::Ammo ? 30 : 1});
        }
}

const WorldItem* LootSystem::Target() const
{
    return target >= 0 ? &items[static_cast<std::size_t>(target)] : nullptr;
}

void LootSystem::Update(float dt, Player& player, const Camera& camera, const CollisionWorld& world,
    bool inputEnabled, bool pickupPressed)
{
    messageTime = (std::max)(0.0f, messageTime - dt);
    if (messageTime == 0) message.clear();
    target = -1;
    if (!inputEnabled) return;
    float nearest = 15.0f;
    const glm::vec3 direction = glm::normalize(camera.front);
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        float hit = 0;
        const auto& item = items[i];
        if (RayBox(camera.position, direction, item.position - glm::vec3(WorldItem::HalfSize),
            item.position + glm::vec3(WorldItem::HalfSize), nearest, hit))
        {
            nearest = hit;
            target = static_cast<int>(i);
        }
    }
    if (target < 0) return;
    const auto& item = items[static_cast<std::size_t>(target)];
    const glm::vec3 eye = player.position + glm::vec3(0, player.IsCrouching() ?
        player.GetSettings().crouchingEyeHeight : player.GetSettings().standingEyeHeight, 0);
    const glm::vec3 toItem = item.position - eye;
    const float reach = glm::length(toItem);
    if (reach > 2.5f || Blocked(world, camera.position, direction, nearest) ||
        (reach > 0.0001f && Blocked(world, eye, toItem / reach, reach)))
    {
        target = -1;
        return;
    }
    if (!pickupPressed) return;
    if (player.GetInventory().TryAdd(item.stack))
    {
        message = std::string("Picked up: ") + GetItemDefinition(item.stack.type).name;
        items.erase(items.begin() + target);
        target = -1;
    }
    else message = "Inventory full - item left on the ground";
    messageTime = 2.5f;
}

void LootSystem::Render(Shader& shader, Mesh& mesh, Camera& camera, float width, float height)
{
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        glm::mat4 model = glm::translate(glm::mat4(1), items[i].position);
        model = glm::scale(model, glm::vec3(WorldItem::HalfSize * 2));
        const glm::vec3 color = static_cast<int>(i) == target ? glm::vec3(1, 0.85f, 0.2f) :
            items[i].stack.type == ItemType::Bandage ? glm::vec3(0.9f, 0.35f, 0.35f) :
            items[i].stack.type == ItemType::Water ? glm::vec3(0.25f, 0.6f, 1) : glm::vec3(0.7f, 0.6f, 0.35f);
        Renderer::Draw(shader, mesh, camera, model, width, height, color);
    }
}

void LootSystem::Reset()
{
    items.clear();
    target = -1;
    message.clear();
    messageTime = 0;
}

void LootSystem::Spawn(glm::vec3 center, ItemStack stack)
{
    if (stack.quantity <= 0) return;
    items.emplace_back(center, ItemStack{stack.type, stack.quantity});
    target = -1;
}

bool LootSystem::Drop(Player& player, StackId id, bool all, const Camera& camera, const CollisionWorld& world)
{
    const auto* entry = player.GetInventory().Find(id);
    if (!entry || !player.IsGrounded()) { Notify("Cannot drop while airborne or without a selection"); return false; }
    const ItemStack dropped{entry->type, all ? entry->quantity : 1};
    glm::vec3 forward(camera.front.x, 0, camera.front.z);
    if (glm::length(forward) < 0.001f) forward = {0, 0, -1};
    forward = glm::normalize(forward);
    const glm::vec3 right(-forward.z, 0, forward.x);
    const glm::vec3 directions[] = {forward, (forward + right) * 0.70710678f, (forward - right) * 0.70710678f};
    constexpr float half = WorldItem::HalfSize;
    for (const auto& direction : directions)
    {
        glm::vec3 center = player.position + direction;
        float top = -1.0e30f;
        for (const auto& box : world.GetBoxes())
            if (center.x - half >= box.min.x && center.x + half <= box.max.x &&
                center.z - half >= box.min.z && center.z + half <= box.max.z &&
                box.max.y <= player.position.y + 0.5f && box.max.y >= player.position.y - 1.5f)
                top = (std::max)(top, box.max.y);
        if (top < -1.0e20f) continue;
        center.y = top + half + 0.001f; // Avoid roundoff overlap with the supporting surface.
        if (!world.CanOccupy(center - glm::vec3(0, half, 0), half, half * 2)) continue;
        const auto overlaps = [&](glm::vec3 low, glm::vec3 high) {
            return center.x + half > low.x && center.x - half < high.x &&
                center.y + half > low.y && center.y - half < high.y &&
                center.z + half > low.z && center.z - half < high.z;
        };
        const float radius = player.GetSettings().bodyRadius;
        bool blocked = overlaps(player.position - glm::vec3(radius, 0, radius),
            player.position + glm::vec3(radius, player.GetBodyHeight(), radius));
        for (const auto& item : items)
            blocked |= overlaps(item.position - glm::vec3(half), item.position + glm::vec3(half));
        const glm::vec3 origin = player.position + glm::vec3(0, half + 0.01f, 0);
        const glm::vec3 delta = center + glm::vec3(0, 0.01f, 0) - origin;
        const float distance = glm::length(delta);
        float hit = 0;
        for (const auto& box : world.GetBoxes())
            blocked |= RayBox(origin, delta / distance, box.min - glm::vec3(half),
                box.max + glm::vec3(half), distance, hit);
        if (blocked) continue;
        // Allocation precedes removal; after reserve both changes are non-allocating.
        items.reserve(items.size() + 1);
        if (!player.GetInventory().Remove(id, dropped.quantity)) return false;
        Spawn(center, dropped);
        Notify("Item dropped");
        return true;
    }
    Notify("No clear supported space to drop the item");
    return false;
}
