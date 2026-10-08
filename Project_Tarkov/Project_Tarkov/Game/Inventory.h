#pragma once
#include <cstddef>
#include <vector>
#include <cstdint>

enum class ItemType { Bandage, Water, Ammo };
struct ItemDefinition { const char* name; int maxStack; float weight; };
const ItemDefinition& GetItemDefinition(ItemType type);
using StackId = std::uint64_t;
struct ItemStack { ItemType type; int quantity; StackId id = 0; };

class Inventory
{
public:
    static constexpr std::size_t Capacity = 12;
    // Adds the entire stack or leaves the inventory unchanged.
    bool TryAdd(ItemStack stack);
    const std::vector<ItemStack>& Items() const { return items; }
    float TotalWeight() const;
    const ItemStack* Find(StackId id) const;
    bool Remove(StackId id, int quantity);
    int Count(ItemType type) const;
    int Take(ItemType type, int quantity);
private:
    std::vector<ItemStack> items;
    StackId nextId = 1;
};
