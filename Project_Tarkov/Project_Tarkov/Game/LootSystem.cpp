#include "LootSystem.h"
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
    // Centers sit on the existing floor at y = -0.9.
    items.emplace_back(glm::vec3(-1, -0.7f, -2), ItemStack{ ItemType::Bandage, 2 });
    items.emplace_back(glm::vec3(0, -0.7f, -2.5f), ItemStack{ ItemType::Water, 1 });
    items.emplace_back(glm::vec3(1, -0.7f, -2), ItemStack{ ItemType::Ammo, 30 });
    items.emplace_back(glm::vec3(2, -0.7f, -3), ItemStack{ ItemType::Ammo, 40 });
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
