#pragma once

#include "../Inventory/Inventory.h"
#include <array>
#include <string>
#include <vector>

namespace Trading {
constexpr int TraderCount = 3;
constexpr int TraderStashCapacity = 80;
constexpr int StageColumns = 6;
constexpr int StageRows = 4;

enum class TraderId : int { WeaponAmmo, MedicalFood, ClothingGear };
enum class Tab : int { Trade, Quests };
enum class Objective : int { DeliverItem, KillEnemies, VisitLocation, ExtractSuccess };
enum class QuestStatus : int { Available, Active, ReadyToComplete, Completed };

struct TraderState {
    TraderId id = TraderId::WeaponAmmo;
    std::string name;
    std::string portraitAssetKey;
    int loyalty = 1;
    Inventory stash;
};

struct QuestDef {
    int id = 0;
    TraderId trader = TraderId::WeaponAmmo;
    int personalOwner = -1; // -1 is a shared trader quest, 0..2 are rescued SCAVs.
    std::string title, description, intro, active, complete;
    Objective objective = Objective::DeliverItem;
    ItemType targetItem = ItemType::Bandage;
    int required = 1, rewardRubles = 0, rewardItemCount = 0;
    ItemType rewardItem = ItemType::Bandage;
    float targetX = 0.f, targetZ = 0.f, targetRadius = 10.f;
};

struct QuestProgress {
    int questId = 0;
    QuestStatus status = QuestStatus::Available;
    int count = 0;
};

struct StagedItem {
    ItemStack stack{ItemType::Bandage, 0};
    int stageX = 0, stageY = 0;
    int originSlot = -100;
};

struct TradeSession {
    bool open = false;
    TraderId trader = TraderId::WeaponAmmo;
    Tab tab = Tab::Trade;
    std::vector<StagedItem> buy, sell;
    std::string message;
};

struct ScavProfile {
    const char* name = "";
    bool unlocked = false;
    bool deployLocked = false;
    int assignedRoom = -1;
};
}
