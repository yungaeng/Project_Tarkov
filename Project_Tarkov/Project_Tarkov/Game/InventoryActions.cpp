#include "InventoryActions.h"
#include "Player.h"
#include "LootSystem.h"
#include <string>

void InventoryActions::Update(Player& player, LootSystem& loot, const Camera& camera, const CollisionWorld& world, bool enabled)
{
    if (!enabled || !player.vitals.Alive()) { Cancel(); return; }
    if (healingMode) {
        const auto* bandage = player.GetInventory().Find(healingStack);
        if (!bandage || bandage->type != ItemType::Bandage) {
            healingMode = false;
            healingStack = 0;
        }
    }
    const auto action = std::exchange(pending, InventoryAction::None);
    auto& inventory = player.GetInventory();
    if (action == InventoryAction::HealPart) {
        const auto* stack = inventory.Find(healingStack);
        auto next = player.vitals;
        if (stack && stack->type == ItemType::Bandage && next.Heal(bodyTarget) &&
            inventory.Remove(healingStack, 1)) {
            player.vitals = next;
            loot.Notify(std::string(BodyPartNames[static_cast<int>(bodyTarget)]) + " 부위를 치료했습니다");
            healingMode = false;
            healingStack = 0;
            if (!inventory.Find(selected)) selected = 0;
        } else loot.Notify("치료 불가: 붕대가 없거나 해당 부위가 손상되지 않았습니다");
        return;
    }
    if (action == InventoryAction::UnequipClothing) {
        loot.Notify(inventory.UnequipClothing(clothingTarget, player.clothingMask) ?
            "의상을 해제해 인벤토리에 보관했습니다" : "의상 해제 불가: 인벤토리 공간을 확인하세요");
        return;
    }
    if (const auto* stack = inventory.Find(target))
    {
        if (action == InventoryAction::Use)
        {
            if (IsClothing(stack->type)) {
                loot.Notify(inventory.EquipClothing(target, player.clothingMask) ?
                    "의상을 착용했습니다" : "이미 같은 부위의 의상을 착용하고 있습니다");
                if (!inventory.Find(selected)) selected = 0;
                return;
            }
            if (stack->type == ItemType::Bandage) {
                if (player.vitals.Health() >= 100) {
                    loot.Notify("치료할 부위가 없습니다");
                    return;
                }
                healingStack = target;
                healingMode = true;
                loot.Notify("좌측 상단의 피격 부위를 클릭해 치료하세요");
                return;
            }
            auto next = player.vitals;
            const bool applied = stack->type == ItemType::Water ? next.Drink() : false;
            if (applied && inventory.Remove(target, 1)) { player.vitals = next; loot.Notify("아이템을 사용했습니다"); }
            else loot.Notify("사용 불가: 회복할 필요가 없거나 탄약 아이템입니다");
        }
        else if (action == InventoryAction::DropOne || action == InventoryAction::DropAll)
            loot.Drop(player, target, action == InventoryAction::DropAll, camera, world);
    }
    if (!inventory.Find(selected)) selected = 0;
}
