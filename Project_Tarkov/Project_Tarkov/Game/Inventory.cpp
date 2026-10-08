#include "Inventory.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

const ItemDefinition& GetItemDefinition(ItemType type)
{
    static const ItemDefinition bandage{ "Bandage", 5, 0.1f };
    static const ItemDefinition water{ "Water", 2, 0.6f };
    static const ItemDefinition ammo{ "5.45 Ammo", 60, 0.01f };
    switch (type)
    {
    case ItemType::Bandage: return bandage;
    case ItemType::Water: return water;
    case ItemType::Ammo: return ammo;
    }
    throw std::invalid_argument("Unknown item type");
}

bool Inventory::TryAdd(ItemStack stack)
{
    if (stack.quantity <= 0) return false;
    const int limit = GetItemDefinition(stack.type).maxStack;
    // Work on a copy so failed pickups never partially consume a world item.
    auto next = items;
    for (auto& entry : next)
    {
        if (entry.type != stack.type) continue;
        const int amount = (std::min)(limit - entry.quantity, stack.quantity);
        entry.quantity += amount;
        stack.quantity -= amount;
        if (stack.quantity == 0) break;
    }
    while (stack.quantity > 0 && next.size() < Capacity)
    {
        const int amount = (std::min)(limit, stack.quantity);
        next.push_back({ stack.type, amount });
        stack.quantity -= amount;
    }
    if (stack.quantity > 0) return false;
    items = std::move(next);
    return true;
}

float Inventory::TotalWeight() const
{
    float weight = 0;
    for (const auto& entry : items)
        weight += GetItemDefinition(entry.type).weight * entry.quantity;
    return weight;
}
