#pragma once
#include "PlayerAppearance.h"
#include "Inventory.h"
#include "PlayerVitals.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct StashPlacement { int type, x, y, width, height; };
enum class HideoutFurniture { Refrigerator, Armory, Wardrobe, MedicalTable, Count };

// v10 inventories own weapons and magazines; legacy counts are used only for migration.
class Hideout
{
public:
    static constexpr int StashColumns = 10;
    static constexpr int StashRows = 8;
    // Legacy item counts used when converting pre-v10 save files.
    std::array<int, ItemTypeCount> stash{{10, 3, 0, 0, 0, 0, 0, 0, 0, 1, 0}};
    PlayerAppearance appearance = PlayerAppearance::Female;
    PlayerVitals vitals;
    std::int64_t rubles = 0;
    std::uint32_t clothingMask = DefaultClothingMask;
    int rifles = 0;
    Inventory loadout;
    Inventory storage;
    static constexpr int HideoutRoomCount = 2;
    static constexpr int FurnitureCount = static_cast<int>(HideoutFurniture::Count);
    std::array<Inventory, HideoutRoomCount * FurnitureCount> furnitureStorage;
    static constexpr int FurnitureStorageIndex(int room, HideoutFurniture furniture) {
        return room * FurnitureCount + static_cast<int>(furniture);
    }
    static constexpr int FurnitureStorageSource(int room, HideoutFurniture furniture) {
        return 10 + FurnitureStorageIndex(room, furniture);
    }
    static constexpr StorageFilter FilterFor(HideoutFurniture furniture) {
        switch (furniture) {
        case HideoutFurniture::Refrigerator: return StorageFilter::Consumables;
        case HideoutFurniture::Armory: return StorageFilter::Armory;
        case HideoutFurniture::Wardrobe: return StorageFilter::Clothing;
        case HideoutFurniture::MedicalTable: return StorageFilter::Medical;
        default: return StorageFilter::None;
        }
    }
    Inventory& FurnitureStorageAt(int room, HideoutFurniture furniture) {
        return furnitureStorage[FurnitureStorageIndex(room, furniture)];
    }
    const Inventory& FurnitureStorageAt(int room, HideoutFurniture furniture) const {
        return furnitureStorage[FurnitureStorageIndex(room, furniture)];
    }
    bool TransferFurnitureItem(int room, HideoutFurniture furniture, StackId id, bool store, int slot = -100);
    bool SellFurnitureItem(int room, HideoutFurniture furniture, StackId id);
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
private:
    static bool SeedStarterSupplies(Hideout& hideout);
};
