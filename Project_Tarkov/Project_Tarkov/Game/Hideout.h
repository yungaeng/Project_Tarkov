#pragma once
#include "Inventory.h"
#include "PlayerVitals.h"
#include <array>
#include <string>
#include <vector>

struct StashPlacement { int type, x, y, width, height; };

// Weapon ownership is separate from the existing consumable inventory.
class Hideout
{
public:
    // Starter totals include worn clothing and the loaded rifle; no duplicates.
    // Bandage, water, ammo, shirt, trousers, underwear, vest, helmet, backpack, boots.
    std::array<int, ItemTypeCount> stash{{10, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0}};
    PlayerVitals vitals;
    std::uint32_t clothingMask = DefaultClothingMask;
    int rifles = 0;
    std::vector<StashPlacement> stashLayout;
    Inventory loadout;
    bool rifle = true;
    int magazine = 30;
    int spawn = 0;
    bool ready = false;
    std::string message;
    void Load();
    bool Save(bool requireStashSpace = false);
    void Transfer(ItemType type, int quantity, bool take);
    void StoreStack(StackId id);
    void ToggleRifle();
    void ToggleClothing(ItemType type, bool fromStash = false);
    void LoadMagazine();
    bool HealBodyPart(BodyPart part);
    std::vector<StashPlacement> StashLayout() const;
    bool MoveStashItem(int sourceX, int sourceY, int destinationX, int destinationY);
    bool AutoArrangeStash();
    bool Depart();
    void Recover(const Inventory& items, bool weapon, int rounds, std::uint32_t clothing);
private:
    bool NormalizeStash();
};
