#pragma once
#include <cstddef>
#include <vector>
#include <cstdint>

enum class ItemType { Bandage, Water, Ammo, Shirt, Skirt, UnderShirt, Underpants, Count };
inline constexpr int ItemTypeCount = static_cast<int>(ItemType::Count);
inline constexpr int ClothingSlotCount = 4;
inline constexpr std::uint32_t AllClothingMask = (1u << ClothingSlotCount) - 1;
inline bool IsClothing(ItemType type) { return type >= ItemType::Shirt && type <= ItemType::Underpants; }
inline std::uint32_t ClothingBit(ItemType type) {
    return IsClothing(type) ? 1u << (static_cast<int>(type) - static_cast<int>(ItemType::Shirt)) : 0;
}
inline ItemType ClothingType(int slot) { return static_cast<ItemType>(static_cast<int>(ItemType::Shirt) + slot); }
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
    bool EquipClothing(StackId id, std::uint32_t& equipped);
    bool UnequipClothing(ItemType type, std::uint32_t& equipped);
private:
    std::vector<ItemStack> items;
    StackId nextId = 1;
};
