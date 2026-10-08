#pragma once

class Player;
class Camera;

class PlayerController
{
public:
    void UpdateInterface();
    void UpdateMovement(Player& player, const Camera& camera) const;
    bool IsGameplayInputEnabled() const;
    bool IsInventoryOpen() const { return inventoryOpen; }
    void Reset();

private:
    bool inventoryOpen = false;
};
