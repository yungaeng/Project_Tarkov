#pragma once
#include "Inventory.h"
#include "../Character/PlayerVitals.h"
namespace InventoryCommands {
// A single deferred command avoids mutating containers while ImGui iterates them.
struct Drag { int source; StackId id; };
enum class Action { None, Move, Use, Heal, FillOne, FillAll, Unload, Eject, Sell, Split };
struct Command {
    Action action = Action::None;
    Drag item{};
    int destination = 0;
    int slot = -100;
    int quantity = 0;
};
bool Execute(Command command, Inventory& carried, Inventory* external, std::uint32_t& clothing, PlayerVitals& vitals, bool male=false);
}
