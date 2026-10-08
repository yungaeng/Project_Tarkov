#pragma once
#include <cstddef>
#include <vector>

enum class ItemType { Bandage, Water, Ammo };
struct ItemDefinition { const char* name; int maxStack; float weight; };
const ItemDefinition& GetItemDefinition(ItemType type);
struct ItemStack { ItemType type; int quantity; };

class Inventory
{
public:
    static constexpr std::size_t Capacity = 12;
    // Adds the entire stack or leaves the inventory unchanged.
    bool TryAdd(ItemStack stack);
    const std::vector<ItemStack>& Items() const { return items; }
    float TotalWeight() const;
private:
    std::vector<ItemStack> items;
};
