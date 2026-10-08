#pragma once
struct GLFWwindow;
class Player;
class CombatSystem;
class InventoryActions;
class LootSystem;

class RaidHud
{
public:
    RaidHud() = default;
    ~RaidHud();
    RaidHud(const RaidHud&) = delete;
    RaidHud& operator=(const RaidHud&) = delete;
    void Init(GLFWwindow* window);
    void Render(const Player& player, const LootSystem& loot, const CombatSystem& combat, InventoryActions& actions, bool inventoryOpen);
    void Shutdown();
private:
    bool contextReady = false;
    bool windowReady = false;
    bool rendererReady = false;
};
