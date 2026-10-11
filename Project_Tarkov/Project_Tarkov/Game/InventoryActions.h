#pragma once
#include "Inventory.h"
#include "PlayerVitals.h"
#include <utility>
class Player;
class LootSystem;
class Camera;
class CollisionWorld;
enum class InventoryAction { None, Use, DropOne, DropAll, UnequipClothing, HealPart };
class InventoryActions
{
public:
    StackId selected = 0;
    bool healingMode = false;
    void Request(InventoryAction action) { if (pending == InventoryAction::None) { pending = action; target = selected; } }
    void RequestUnequip(ItemType type) { if (pending == InventoryAction::None && IsClothing(type)) { pending = InventoryAction::UnequipClothing; clothingTarget = type; } }
    void RequestHeal(BodyPart part) { if (pending == InventoryAction::None && healingMode) { pending = InventoryAction::HealPart; bodyTarget = part; } }
    void Cancel() { pending = InventoryAction::None; selected = target = healingStack = 0; healingMode = false; }
    void Update(Player& player, LootSystem& loot, const Camera& camera, const CollisionWorld& world, bool enabled);
private:
    InventoryAction pending = InventoryAction::None;
    StackId target = 0;
    StackId healingStack = 0;
    ItemType clothingTarget = ItemType::Shirt;
    BodyPart bodyTarget = BodyPart::Head;
};
