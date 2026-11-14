#pragma once
#include "../Character/PlayerAppearance.h"
#include "../Inventory/Inventory.h"
#include "../Character/PlayerVitals.h"
#include "../Combat/LootSystem.h"
#include "../Trading/TradingSystem.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace InventoryCommands { struct Command; }

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
    PlayerAppearance appearance = PlayerAppearance::Male;
    PlayerVitals vitals;
    std::int64_t rubles = 0;
    std::uint32_t clothingMask = DefaultClothingMask;
    int rifles = 0;
    Inventory loadout;
    Inventory storage;
    LootSystem floorLoot;
    static constexpr int HideoutRoomCount = 4;
    static constexpr int InitialRoomCount = 2;
    static constexpr int MaxRescuedSurvivors = HideoutRoomCount-InitialRoomCount;
    int rescuedSurvivors = 0;
    std::uint32_t rescuedRoomMask = 0;
    static constexpr int FurnitureCount = static_cast<int>(HideoutFurniture::Count);
    // Interaction ID only; keep the persisted furniture array unchanged.
    static constexpr int PelicanStorage = FurnitureCount;
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
    bool HasRescuedSurvivor(int room) const {
        return room >= InitialRoomCount && room < HideoutRoomCount &&
            (rescuedRoomMask & (1u << (room-InitialRoomCount))) != 0;
    }
    int AvailableRoomCount() const {
        int count=InitialRoomCount;
        for (int room=InitialRoomCount;room<HideoutRoomCount;++room)
            if (HasRescuedSurvivor(room)) count=room+1;
        return count;
    }
    bool TransferFurnitureItem(int room, HideoutFurniture furniture, StackId id, bool store, int slot = -100);
    bool ConsolidateEquipmentStorage();
    bool DropToFloor(const InventoryCommands::Command& command, const glm::vec3& position, const CollisionWorld& world);
    bool TakeFromFloor(const InventoryCommands::Command& command, Player& player, const CollisionWorld& world);
    bool SellFurnitureItem(int room, HideoutFurniture furniture, StackId id);
    bool DrinkWater(StackId id = 0, bool fromStash = true);
    void RefreshWeapons();
    bool rifle = true;
    int magazine = 30;
    int spawn = 0;
    bool ready = false;
    // Current prototype has one playable SCAV appearance. The value is saved so
    // that a restart cannot bypass its one PMC-deployment cooldown.
    bool scavDeployLocked = false;
    Trading::TradingSystem trading;
    std::array<Trading::ScavProfile, 3> scavProfiles{{
        {"서윤", true, false, 1}, {"지아", false, false, -1}, {"나린", false, false, -1}
    }};
    int activeScav = 0;
    std::string message;
    void Load();
    bool ChangeAppearance(PlayerAppearance next, int releasedSurvivorRoom = -1);
    bool Save(bool requireStashSpace = false);
    void ApplyInventoryCommand(const InventoryCommands::Command& command, int storageRoom = -1, int storageFurniture = -1);
    bool HealBodyPart(BodyPart part, bool fromStash = true);
    bool AutoArrangeStash(int room = -1, int furniture = -1);
    bool SellStashItem(StackId id);
    bool Depart();
    bool SelectScav(int index);
    bool Recover(const Inventory& items, bool weapon, int rounds, std::uint32_t clothing,
        bool rescuedSurvivor = false);
private:
    static bool SeedStarterSupplies(Hideout& hideout);
    static bool SeedRoomSupplies(Hideout& hideout, int room);
};
