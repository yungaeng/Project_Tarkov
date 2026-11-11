#pragma once
#include "../Character/Entity.h"
#include "../Inventory/Inventory.h"
#include <string>
#include <vector>

class Player;
class Camera;
class CollisionWorld;
class Shader;
class Mesh;
struct WorldItem : Entity
{
    ItemStack stack;
    WorldItem(glm::vec3 center, ItemStack contents) : stack(contents) { position = center; }
    static constexpr float HalfSize = 0.2f;
};

class LootSystem
{
public:
    void Init();
    void Update(float dt, Player& player, const Camera& camera, const CollisionWorld& world,
        bool inputEnabled, bool pickupPressed);
    void Render(Shader& shader, Mesh& mesh, Camera& camera, float width, float height);
    const WorldItem* Target() const;
    const std::string& Message() const { return message; }
    void Reset();
    bool Drop(Player& player, StackId id, bool all, const Camera& camera, const CollisionWorld& world);
    void Spawn(glm::vec3 center, ItemStack stack);
    bool DropNear(glm::vec3 position, ItemStack stack, const CollisionWorld& world);
    std::vector<ItemStack> Nearby(const Player& player, const CollisionWorld& world) const;
    bool TakeNearby(StackId id, Player& player, const CollisionWorld& world, int slot = -100);
    bool EquipNearby(StackId id, Player& player, const CollisionWorld& world, int clothingSlot = -1);
    void Notify(const std::string& text) { message = text; messageTime = 3; }
private:
    std::vector<WorldItem> items;
    int target = -1;
    StackId nextWorldId = 1;
    std::string message;
    float messageTime = 0;
};
