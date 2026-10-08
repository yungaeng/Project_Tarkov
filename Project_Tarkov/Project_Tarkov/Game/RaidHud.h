#pragma once
struct GLFWwindow;
class Inventory;
class LootSystem;

class RaidHud
{
public:
    RaidHud() = default;
    ~RaidHud();
    RaidHud(const RaidHud&) = delete;
    RaidHud& operator=(const RaidHud&) = delete;
    void Init(GLFWwindow* window);
    void Render(const Inventory& inventory, const LootSystem& loot, bool inventoryOpen);
    void Shutdown();
private:
    bool contextReady = false;
    bool windowReady = false;
    bool rendererReady = false;
};
