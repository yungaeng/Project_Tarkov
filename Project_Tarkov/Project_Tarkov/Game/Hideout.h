#pragma once
#include "Inventory.h"
#include <array>
#include <string>

// Weapon ownership is separate from the existing consumable inventory.
class Hideout
{
public:
    std::array<int, ItemTypeCount> stash{{50, 50, 50, 50, 50, 50, 50, 50, 50, 50}};
    std::uint32_t clothingMask = DefaultClothingMask;
    int rifles = 50;
    Inventory loadout;
    bool rifle = false;
    int magazine = 0;
    int spawn = 0;
    bool ready = false;
    std::string message;
    void Load();
    bool Save();
    void Transfer(ItemType type, int quantity, bool take);
    void StoreStack(StackId id);
    void ToggleRifle();
    void ToggleClothing(ItemType type, bool fromStash = false);
    void LoadMagazine();
    bool Depart();
    void Recover(const Inventory& items, bool weapon, int rounds, std::uint32_t clothing);
};
