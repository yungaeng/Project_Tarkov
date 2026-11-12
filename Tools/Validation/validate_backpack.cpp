#include "../../Project_Tarkov/src/Game/Inventory/InventoryCommands.h"
#include <cassert>
#include <iostream>

int main()
{
    auto mask = ClothingBit(ItemType::Backpack) | ClothingBit(ItemType::Vest);
    Inventory carried, stash;
    PlayerVitals vitals;
    assert(carried.ConfigureGear(mask));
    assert(stash.SetSlotCapacity(80));
    assert(carried.TryAdd({ItemType::Water, 2, 0, 0, false, 18}));
    assert(carried.TryAdd({ItemType::Magazine, 1, 0, 17, false, 20}));
    assert(carried.TryAdd({ItemType::Bandage, 1, 0, 0, false, 0}));
    const auto original = carried;
    const auto originalMask = mask;
    const InventoryCommands::Command remove{InventoryCommands::Action::Move,
        {2, static_cast<StackId>(ClothingSlot(ItemType::Backpack)+1)}, 1};
    assert(InventoryCommands::Execute(remove, carried, &stash, mask, vitals));
    assert(!(mask & ClothingBit(ItemType::Backpack)));
    assert(mask & ClothingBit(ItemType::Vest));
    assert(carried.Count(ItemType::Bandage) == 1 && carried.Count(ItemType::Water) == 0);
    assert(stash.Count(ItemType::Backpack) == 1 && stash.Count(ItemType::Water) == 2);
    for (const auto& item : stash.Items())
        if (item.type == ItemType::Magazine) assert(item.rounds == 17);

    carried = original; mask = originalMask;
    Inventory full;
    assert(full.SetSlotCapacity(80));
    for (int slot=0; slot<80; ++slot)
        assert(full.TryAdd({ItemType::Bandage, 5, 0, 0, false, slot}));
    assert(!InventoryCommands::Execute(remove, carried, &full, mask, vitals));
    assert(mask == originalMask && carried.At(18) && carried.At(20));
    assert(full.Count(ItemType::Backpack) == 0 && full.Count(ItemType::Bandage) == 400);

    // Leave exactly enough room for the bag, but none for its contents.
    for (int slot : {0,1,2,10,11,12,20,21,22}) {
        const auto id = full.At(slot)->id;
        assert(full.Remove(id, 5));
    }
    assert(!InventoryCommands::Execute(remove, carried, &full, mask, vitals));
    assert(mask == originalMask && carried.At(18) && carried.At(20));
    assert(full.Count(ItemType::Backpack) == 0 && full.Count(ItemType::Bandage) == 355);

    std::vector<ItemStack> dropped;
    assert(carried.DetachBackpack(mask, dropped));
    assert(dropped.size() == 3 && dropped.front().type == ItemType::Backpack);
    assert(carried.Count(ItemType::Bandage) == 1 && carried.Count(ItemType::Magazine) == 0);
    assert(!carried.DetachBackpack(mask, dropped));
    Inventory empty;
    mask = ClothingBit(ItemType::Backpack);
    assert(empty.ConfigureGear(mask));
    assert(empty.DetachBackpack(mask, dropped) && dropped.size() == 1 && mask == 0);
    std::cout << "Backpack transfer and rollback checks passed\n";
}
