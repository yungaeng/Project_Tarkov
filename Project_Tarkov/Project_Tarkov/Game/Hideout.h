#pragma once
#include "PlayerAppearance.h"
#include "Inventory.h"
#include "PlayerVitals.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct StashPlacement { int type, x, y, width, height; };

// v10 inventories own weapons and magazines; legacy counts are used only for migration.
class Hideout
{
public:
    static constexpr int StashColumns = 10;
    static constexpr int StashRows = 8;
    // Starter totals include worn clothing and the loaded rifle; no duplicates.
    // Bandage, water, ammo, shirt, trousers, underwear, vest, helmet, backpack, boots.
    std::array<int, ItemTypeCount> stash{{10, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0}};
    PlayerAppearance appearance = PlayerAppearance::Female;
    PlayerVitals vitals;
    std::int64_t rubles = 0;
    std::uint32_t clothingMask = DefaultClothingMask;
    int rifles = 0;
    Inventory loadout;
    Inventory storage;
    bool DrinkWater(StackId id = 0, bool fromStash = true);
    void RefreshWeapons();
    bool rifle = true;
    int magazine = 30;
    int spawn = 0;
    bool ready = false;
    std::string message;
    void Load();
    bool ChangeAppearance(PlayerAppearance next);
    bool Save(bool requireStashSpace = false);
    bool HealBodyPart(BodyPart part);
    bool AutoArrangeStash();
    bool SellStashItem(StackId id);
    bool Depart();
    void Recover(const Inventory& items, bool weapon, int rounds, std::uint32_t clothing);
};
