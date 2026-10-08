#pragma once
#include "Character.h"
#include "Inventory.h"

// Inventory data belongs to the player, independently of world item entities.
class Player : public Character
{
public:
    Inventory& GetInventory() { return inventory; }
    const Inventory& GetInventory() const { return inventory; }
private:
    Inventory inventory;
};
