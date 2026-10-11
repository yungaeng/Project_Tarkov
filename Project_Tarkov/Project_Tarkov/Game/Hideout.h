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
    std::array<int, ItemTypeCount> stash{{10, 10, 10, 10, 10, 10, 10, 10, 10, 10}};
    PlayerVitals vitals;
    std::uint32_t clothingMask = DefaultClothingMask;
    int rifles = 10;
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
    bool HealBodyPart(BodyPart part, ItemType medicine = ItemType::Bandage);
    std::vector<StashPlacement> StashLayout() const;
    bool Depart();
    void Recover(const Inventory& items, bool weapon, int rounds, std::uint32_t clothing);
private:
    void NormalizeStash();
};
