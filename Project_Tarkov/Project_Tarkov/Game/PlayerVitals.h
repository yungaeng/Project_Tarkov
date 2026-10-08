#pragma once
#include <algorithm>

class PlayerVitals
{
public:
    float Health() const { return health; }
    float Hydration() const { return hydration; }
    float Stamina() const { return stamina; }
    bool Alive() const { return health > 0; }
    bool CanSprint() const { return !exhausted && stamina > 0 && Alive(); }
    void Damage(float amount) { health = (std::max)(0.0f, health - (std::max)(0.0f, amount)); }
    bool Heal() { if (!Alive() || health >= 100) return false; health = (std::min)(100.0f, health + 20); return true; }
    bool Drink() { if (!Alive() || hydration >= 100) return false; hydration = (std::min)(100.0f, hydration + 30); return true; }
    void Update(float dt, bool sprinting)
    {
        if (!Alive()) return;
        hydration = (std::max)(0.0f, hydration - dt * 0.1f);
        stamina = std::clamp(stamina + dt * (sprinting ? -22.0f : 14.0f), 0.0f, 100.0f);
        if (stamina <= 0) exhausted = true;
        if (stamina >= 20) exhausted = false;
        if (hydration <= 0) Damage(dt);
    }
private:
    float health = 100, hydration = 100, stamina = 100;
    bool exhausted = false;
};
