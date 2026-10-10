#include "Inventory.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

const ItemDefinition& GetItemDefinition(ItemType type)
{
    static const ItemDefinition bandage{ "붕대", 5, 0.1f };
    static const ItemDefinition water{ "물", 2, 0.6f };
    static const ItemDefinition ammo{ "5.45mm 탄약", 60, 0.01f };
    static const ItemDefinition shirt{ "멀티캠 전투 상의", 1, 0.4f };
    static const ItemDefinition trousers{ "멀티캠 전투 하의", 1, 0.6f };
    static const ItemDefinition vest{ "멀티캠 숏 전술조끼", 1, 1.2f };
    static const ItemDefinition helmet{ "멀티캠 방탄헬멧", 1, 1.3f };
    static const ItemDefinition underShirt{ "상의 내의", 1, 0.15f };
    static const ItemDefinition underpants{ "하의 내의", 1, 0.1f };
    switch (type)
    {
    case ItemType::Bandage: return bandage;
    case ItemType::Water: return water;
    case ItemType::Ammo: return ammo;
    case ItemType::Shirt: return shirt;
    case ItemType::Trousers: return trousers;
    case ItemType::Vest: return vest;
    case ItemType::Helmet: return helmet;
    case ItemType::UnderShirt: return underShirt;
    case ItemType::Underpants: return underpants;
    }
    throw std::invalid_argument("Unknown item type");
}

bool Inventory::TryAdd(ItemStack stack)
{
    if (stack.quantity <= 0 || static_cast<int>(stack.type) < 0 || stack.type >= ItemType::Count) return false;
    const int limit = GetItemDefinition(stack.type).maxStack;
    // Work on a copy so failed pickups never partially consume a world item.
    auto next = items;
    auto availableId = nextId;
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
        next.push_back({ stack.type, amount, availableId++ });
        stack.quantity -= amount;
    }
    if (stack.quantity > 0) return false;
    items = std::move(next);
    nextId = availableId;
    return true;
}

const ItemStack* Inventory::Find(StackId id) const
{
    for (const auto& item : items) if (item.id == id) return &item;
    return nullptr;
}

bool Inventory::Remove(StackId id, int quantity)
{
    for (auto it = items.begin(); it != items.end(); ++it)
        if (it->id == id && quantity > 0 && quantity <= it->quantity)
        {
            it->quantity -= quantity;
            if (!it->quantity) items.erase(it);
            return true;
        }
    return false;
}

int Inventory::Count(ItemType type) const
{
    int total = 0;
    for (const auto& item : items) if (item.type == type) total += item.quantity;
    return total;
}

int Inventory::Take(ItemType type, int quantity)
{
    int taken = 0;
    for (auto it = items.begin(); it != items.end() && taken < quantity;)
    {
        if (it->type != type) { ++it; continue; }
        const int amount = (std::min)(quantity - taken, it->quantity);
        taken += amount;
        it->quantity -= amount;
        if (!it->quantity) it = items.erase(it); else ++it;
    }
    return taken;
}

float Inventory::TotalWeight() const
{
    float weight = 0;
    for (const auto& entry : items)
        weight += GetItemDefinition(entry.type).weight * entry.quantity;
    return weight;
}

bool Inventory::EquipClothing(StackId id, std::uint32_t& equipped)
{
    const auto* stack = Find(id);
    if (!stack || !IsClothing(stack->type)) return false;
    const auto bit = ClothingBit(stack->type);
    if ((equipped & bit) || !Remove(id, 1)) return false;
    equipped |= bit;
    return true;
}

bool Inventory::UnequipClothing(ItemType type, std::uint32_t& equipped)
{
    const auto bit = ClothingBit(type);
    if (!bit || !(equipped & bit) || !TryAdd({type, 1})) return false;
    equipped &= ~bit;
    return true;
}
