#pragma once
#include "Character.h"
#include "Inventory.h"
#include "PlayerVitals.h"

// Inventory data belongs to the player, independently of world item entities.
class Player : public Character
{
public:
    PlayerVitals vitals;
    Inventory& GetInventory() { return inventory; }
    const Inventory& GetInventory() const { return inventory; }
private:
    Inventory inventory;
};
