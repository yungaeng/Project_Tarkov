#pragma once
#include "Inventory.h"
#include <utility>
class Player;
class LootSystem;
class Camera;
class CollisionWorld;
enum class InventoryAction { None, Use, DropOne, DropAll };
class InventoryActions
{
public:
    StackId selected = 0;
    void Request(InventoryAction action) { if (pending == InventoryAction::None) { pending = action; target = selected; } }
    void Cancel() { pending = InventoryAction::None; selected = target = 0; }
    void Update(Player& player, LootSystem& loot, const Camera& camera, const CollisionWorld& world, bool enabled);
private:
    InventoryAction pending = InventoryAction::None;
    StackId target = 0;
};
