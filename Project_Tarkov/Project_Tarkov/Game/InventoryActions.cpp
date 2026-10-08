#include "InventoryActions.h"
#include "Player.h"
#include "LootSystem.h"

void InventoryActions::Update(Player& player, LootSystem& loot, const Camera& camera, const CollisionWorld& world, bool enabled)
{
    if (!enabled || !player.vitals.Alive()) { Cancel(); return; }
    const auto action = std::exchange(pending, InventoryAction::None);
    auto& inventory = player.GetInventory();
    if (const auto* stack = inventory.Find(target))
    {
        if (action == InventoryAction::Use)
        {
            auto next = player.vitals;
            const bool applied = stack->type == ItemType::Bandage ? next.Heal() :
                stack->type == ItemType::Water ? next.Drink() : false;
            if (applied && inventory.Remove(target, 1)) { player.vitals = next; loot.Notify("Item used"); }
            else loot.Notify("Cannot use: stat is full or item is ammunition");
        }
        else if (action == InventoryAction::DropOne || action == InventoryAction::DropAll)
            loot.Drop(player, target, action == InventoryAction::DropAll, camera, world);
    }
    if (!inventory.Find(selected)) selected = 0;
}
