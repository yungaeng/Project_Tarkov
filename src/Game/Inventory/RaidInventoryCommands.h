#pragma once
#include "InventoryCommands.h"
class Player;
class LootSystem;
class CollisionWorld;
class InventoryActions;
struct Enemy;
namespace InventoryCommands {
void ApplyRaid(Command command, Player& player, LootSystem& loot, const CollisionWorld& world, InventoryActions& actions, Enemy* corpse);
}
