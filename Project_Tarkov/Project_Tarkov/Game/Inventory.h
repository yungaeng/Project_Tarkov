#pragma once
#include <cstddef>
#include <vector>
#include <cstdint>

// Keep existing numeric IDs for saved inventories (the former skirt slot is trousers).
enum class ItemType { Bandage, Water, Ammo, Shirt, Trousers, UnderShirt, Underpants, Vest, Helmet, Backpack, Boots, Rifle, Magazine, Pistol, PistolMagazine, Trunks, Count };
inline bool IsWeapon(ItemType t) { return t == ItemType::Rifle || t == ItemType::Pistol; }
inline bool IsMagazine(ItemType t) { return t == ItemType::Magazine || t == ItemType::PistolMagazine; }
inline int MagazineCapacity(ItemType t) { return t == ItemType::Pistol || t == ItemType::PistolMagazine ? 15 : 30; }
inline ItemType MagazineType(ItemType t) { return t == ItemType::Pistol ? ItemType::PistolMagazine : ItemType::Magazine; }
enum class FireMode { Semi, Auto, Safe };
inline constexpr int ItemTypeCount = static_cast<int>(ItemType::Count);
inline constexpr int ClothingSlotCount = 8;
inline constexpr std::uint32_t AllClothingMask = (1u << ClothingSlotCount) - 1;
inline bool IsClothing(ItemType type) { return (type >= ItemType::Shirt && type <= ItemType::Boots) || type == ItemType::Trunks; }
inline int ClothingSlot(ItemType t) { return static_cast<int>(t == ItemType::Trunks ? ItemType::Underpants : t) - static_cast<int>(ItemType::Shirt); }
inline bool CanWear(ItemType t, bool male) { return male ? t != ItemType::UnderShirt && t != ItemType::Underpants : t != ItemType::Trunks; }
inline std::uint32_t ClothingBit(ItemType type) {
    return IsClothing(type) ? 1u << ClothingSlot(type) : 0;
}
inline ItemType ClothingType(int slot, bool male=false) { if (male && slot==ClothingSlot(ItemType::Underpants)) return ItemType::Trunks; return static_cast<ItemType>(static_cast<int>(ItemType::Shirt) + slot); }
inline constexpr int BaseInventorySlots = 12;
inline int InventorySlotsForGear(std::uint32_t equipped) {
    return BaseInventorySlots + ((equipped & ClothingBit(ItemType::Vest)) ? 6 : 0) +
        ((equipped & ClothingBit(ItemType::Backpack)) ? 12 : 0);
}
inline constexpr std::uint32_t DefaultClothingMask = AllClothingMask & ~(1u << 6);
struct ItemDefinition { const char* name; int maxStack; float weight; int price; };
const ItemDefinition& GetItemDefinition(ItemType type);
struct ItemSize { int width, height; };
inline ItemSize ItemFootprint(ItemType type) {
    switch (type) {
    case ItemType::Water: case ItemType::Shirt: case ItemType::Trousers:
    case ItemType::UnderShirt: case ItemType::Trunks: case ItemType::Underpants: case ItemType::Magazine: return {1,2};
    case ItemType::Ammo: case ItemType::Pistol: return {2,1};
    case ItemType::Vest: case ItemType::Helmet: case ItemType::Boots: return {2,2};
    case ItemType::Backpack: return {3,3};
    case ItemType::Rifle: return {3,1};
    default: return {1,1};
    }
}
using StackId = std::uint64_t;
// Slots 0..11 pockets, 12..17 vest, 18..29 backpack; -1/-2 primary, -3 sidearm.
// -100 requests automatic placement. IDs remain stable when moving within a container.
struct ItemStack {
    ItemType type; int quantity; StackId id = 0;
    int rounds = 0;
    bool magazineInserted = false;
    int slot = -100;
    FireMode fireMode = FireMode::Semi;
};

class Inventory
{
public:
    static constexpr std::size_t Capacity = BaseInventorySlots;
    // Adds the entire stack or leaves the inventory unchanged.
    bool TryAdd(ItemStack stack);
    const std::vector<ItemStack>& Items() const { return items; }
    float TotalWeight() const;
    const ItemStack* Find(StackId id) const;
    bool Remove(StackId id, int quantity);
    int Count(ItemType type) const;
    int Take(ItemType type, int quantity);
    int SlotCapacity() const { return slotCapacity; }
    bool SetSlotCapacity(int capacity);
    bool EquipClothing(StackId id, std::uint32_t& equipped);
    bool UnequipClothing(ItemType type, std::uint32_t& equipped);
    const ItemStack* At(int slot) const;
    const ItemStack* Covering(int slot) const;
    int Columns(int slot) const { return storage ? 10 : slot < 12 ? 6 : slot < 18 ? 3 : 4; }
    int RegionStart(int slot) const { return storage || slot < 12 ? 0 : slot < 18 ? 12 : 18; }
    bool CanPlace(ItemType type, int slot, StackId ignore = 0) const;
    ItemStack* Edit(StackId id);
    bool Move(StackId id, int slot);
    bool TransferTo(StackId id, Inventory& destination, int slot = -100);
    bool FillMagazine(StackId id, int amount, StackId ammo = 0);
    bool UnloadMagazine(StackId id);
    bool InsertMagazine(StackId magazine, StackId weapon);
    bool EjectMagazine(StackId weapon);
    void AutoArrange();
    StackId BestMagazine(ItemType weapon, std::uint32_t clothing) const;
    bool SlotAllowed(ItemType type, int slot) const;
    bool ConfigureGear(std::uint32_t clothing);
    bool Pickup(ItemStack stack);
    int StorageUsed() const;
private:
    std::vector<ItemStack> items;
    StackId nextId = 1;
    int slotCapacity = BaseInventorySlots;
    std::uint32_t gear = 0;
    bool storage = false;
};
